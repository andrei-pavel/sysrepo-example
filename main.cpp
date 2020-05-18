#include <mutex>
#include <sstream>
#include <string>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

#define ON_CHANGE true
#define ON_UPDATE true

using namespace sysrepo;
using namespace std::chrono_literals;

std::vector<S_Change> CHANGES;
std::mutex MUTEX;

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
    S_Iter_Change iterator(session->get_changes_iter("/model:*//."));
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

#ifdef ON_CHANGE
    if (event == SR_EV_CHANGE) {
      std::thread([&]() {
        S_Connection connection(std::make_shared<Connection>());
        S_Session session(std::make_shared<Session>(connection, SR_DS_RUNNING));
        f(session);
        session->apply_changes();
      }).detach();
    }
#endif

#ifdef ON_UPDATE
    if (event == SR_EV_UPDATE) {
      f(session);
    }
#endif

    return SR_ERR_OK;
  }

  void f(S_Session const &session) {
    S_Val value(session->get_item("/model:config/child"));
    session->delete_item("/model:config");
    session->set_item("/model:config/le_list[name='whatever']/contained/data",
                      std::make_shared<Val>(1337));
    session->set_item_str(
        "/model:config/le_list[name='whatever']/contained/floating", "0.37");
    session->set_item("/model:config/child", std::make_shared<Val>(1337));
    S_Val v(std::make_shared<Val>(nullptr, SR_CONTAINER_T));
    session->set_item("/model:config/le_list[name='a']/"
                      "le_nested_list[a='1'][b='2'][c='3'][d='4']",
                      v);
  }
};

struct SysrepoClient {
  SysrepoClient() {
    connection_ = std::make_shared<Connection>();
    model_ = "model";
    session_ = std::make_shared<Session>(connection_, SR_DS_RUNNING);
    subscription_ = std::make_shared<Subscribe>(session_);
    subscription_->module_change_subscribe(
        model_.c_str(), std::make_shared<SysrepoCallback>(), nullptr, nullptr,
        0, SR_SUBSCR_UPDATE);
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
    libyang::S_Data_Node node(session_->get_data("/model:config"));

    // For each node...
    for (libyang::S_Data_Node const& n : node->tree_dfs()) {
      std::cout << n->path() << std::endl;
    }
  }

  void loop() {
    // Every 1s...
    while (true) {
      {
        std::lock_guard<std::mutex> _(MUTEX);
        displayXpaths();
      }
      std::this_thread::sleep_for(1s);
    }
  }

  S_Connection connection_;
  std::string model_;
  S_Session session_;
  S_Subscribe subscription_;
};

int main() {
  SysrepoClient client;
  client.loop();

  return 0;
}
