#include <mutex>
#include <sstream>
#include <string>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

// #define DISPLAY_CHANGES
#define DISPLAY_XPATHS

using namespace libyang;
using namespace sysrepo;
using namespace std::chrono_literals;

std::vector<S_Change> CHANGES;
std::mutex MUTEX;

std::string const MODEL = "model";
std::string const MODEL_ALL = "/" + MODEL + ":*//.";
std::string const ROOT_NODE = "config";
std::string const MODEL_AND_ROOT_NODE = "/" + MODEL + ":" + ROOT_NODE;

struct SysrepoCallback : Callback {
  int module_change(S_Session session, char const * /* module_name */,
                    char const *xpath, sr_event_t event,
                    uint32_t /* request_id */, void * /* private_ctx */) {
    // Display event type.
    std::ostringstream event_type;
    switch (event) {
    case SR_EV_UPDATE:
      event_type << "SR_EV_UPDATE";
      break;
    case SR_EV_CHANGE:
      event_type << "SR_EV_CHANGE";
      break;
    case SR_EV_DONE:
      event_type << "SR_EV_DONE";
      break;
    case SR_EV_ABORT:
      event_type << "SR_EV_ABORT";
      break;
    case SR_EV_ENABLED:
      event_type << "SR_EV_ENABLED";
      break;
    default:
      event_type << "unknown (" << event << ")";
      break;
    }
    std::cout << event_type.str() << std::endl;

    // Iterate through changes.
    S_Iter_Change iterator(session->get_changes_iter(MODEL_ALL.c_str()));
    if (!iterator) {
      std::cerr << "no iterator" << std::endl;
      return 1;
    }

    // Push changes to global vector.
    std::lock_guard<std::mutex> _(MUTEX);
    while (true) {
      S_Change change;
      try {
        change = session->get_change_next(iterator);
      } catch (sysrepo_exception const &exception) {
        std::cerr << "get change iterator next failed: " << exception.what()
                  << std::endl;
        break;
      }
      if (!change) {
        // End of changes, not an error.
        break;
      }
      CHANGES.push_back(change);
    }

    return SR_ERR_OK;
  }
};

struct SysrepoClient {
  SysrepoClient() {
    connection_ = std::make_shared<Connection>();
    session_ = std::make_shared<Session>(connection_, SR_DS_RUNNING);
    subscription_ = std::make_shared<Subscribe>(session_);
    subscription_->module_change_subscribe(
        MODEL.c_str(), std::make_shared<SysrepoCallback>(), nullptr, nullptr, 0,
        SR_SUBSCR_UPDATE);
  }

  void displayChanges() {
    for (S_Change const &change : CHANGES) {
      std::cout << "operation: " << change->oper() << std::endl;
      S_Val const &o(change->old_val());
      S_Val const &n(change->new_val());
      if (o) {
        std::cout << "old: " << o->to_string() << std::endl;
      }
      if (n) {
        std::cout << "new: " << n->to_string() << std::endl;
      }
      std::cout << std::endl;
    }
    CHANGES.clear();
  }

  void displayXpaths() {
    libyang::S_Data_Node node(session_->get_data(MODEL_AND_ROOT_NODE.c_str()));

    // For each node...
    for (libyang::S_Data_Node const &n : node->tree_dfs()) {
      std::string path(n->path());
      std::string const is_key(isKey(path) ? "is key" : "is not key");
      std::vector<std::string> kk(keys(path));
      std::cout << n->path() << " " << is_key;
      for (std::string const &k : kk) {
        std::cout << " - " << k << std::endl;
      }
      std::cout << std::endl;
    }

    std::cout << std::endl << std::endl;
  }

  void loop() {
    // Every 1s...
    while (true) {
      {
        std::lock_guard<std::mutex> _(MUTEX);
#ifdef DISPLAY_CHANGES
        displayChanges();
#endif
#ifdef DISPLAY_XPATHS
        displayXpaths();
#endif
      }
      std::this_thread::sleep_for(1s);
    }
  }

private:
  S_Set getSet(std::string const& xpath) {
    char const *const &xpath_c_str(xpath.c_str());

    S_Context context(session_->get_context());
    S_Module module(context->get_module(MODEL.c_str()));
    S_Schema_Node data_node(module->data());

    // S_Data_Node const data_node(connection_->get_module_info());
    // S_Data_Node const data_node(session_->get_data(MODEL_ALL.c_str()));
    // S_Data_Node const data_node(session_->get_data(MODEL_AND_ROOT_NODE.c_str()));
    // S_Data_Node const data_node(session_->get_data(xpath_c_str));
    // S_Data_Node const data_node(session_->get_data(xpath_c_str));

    return data_node->find_path(xpath_c_str);
  }

  bool isKey(std::string const &xpath) {
    S_Set const set(getSet(xpath));
    std::cerr << "  isKey(" << xpath << "): " << set->number() << std::endl;
    for (S_Schema_Node const &schema_node : set->schema()) {
      if (schema_node->nodetype() != LYS_LEAF) {
        std::cerr << "  isKey(" << xpath << "): " << schema_node->nodetype()
                  << " != LYS_LEAF" << std::endl;
        continue;
      }
      Schema_Node_Leaf schema_node_leaf(schema_node);
      return schema_node_leaf.is_key() != nullptr;
    }
    return false;
  }

  std::vector<std::string> keys(std::string const &xpath) {
    S_Set const set(getSet(xpath));
    std::cerr << "    keys(" << xpath << "): " << set->number() << std::endl;
    std::vector<std::string> result;
    for (S_Schema_Node const &schema_node : set->schema()) {
      if (schema_node->nodetype() != LYS_LIST) {
        std::cerr << "    keys(" << xpath << "): " << schema_node->nodetype()
                  << " != LYS_LIST" << std::endl;
        continue;
      }
      Schema_Node_List schema_node_list(schema_node);
      for (S_Schema_Node_Leaf const &key_node : schema_node_list.keys()) {
        if (schema_node->nodetype() != LYS_LEAF) {
          std::cerr << "    keys(" << xpath << "): " << schema_node->nodetype()
                    << " != LYS_LEAF" << std::endl;
          continue;
        }
        result.push_back(key_node->name());
      }
    }
    return result;
  }

  S_Connection connection_;
  S_Session session_;
  S_Subscribe subscription_;
};

int main() {
  SysrepoClient client;
  client.loop();

  return 0;
}
