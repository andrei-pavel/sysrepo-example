#include <mutex>
#include <sstream>
#include <string>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

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
    case SR_EV_CHANGE:
      event_type << "change";
      break;
    case SR_EV_DONE:
      event_type << "done";
      break;
    case SR_EV_ABORT:
      event_type << "abort";
      break;
    case SR_EV_ENABLED:
      event_type << "enabled";
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

    // Get an item.
    S_Val value(session->get_item("/model:config/child"));

    std::thread([]() {
      S_Connection connection(std::make_shared<Connection>());
      S_Session session(std::make_shared<Session>(connection, SR_DS_RUNNING));
      session->delete_item("/model:config");
      session->set_item("/model:config/le_list[name='whatever']/contained/data",
                        std::make_shared<Val>(1337));
      session->set_item_str(
          "/model:config/le_list[name='whatever']/contained/floating", "0.37");
      session->apply_changes();
    }).detach();

    return 0;
  }
};

struct SysrepoClient {
  SysrepoClient() {
    connection_ = std::make_shared<Connection>();
    session_ = std::make_shared<Session>(connection_, SR_DS_RUNNING);
    subscription_ = std::make_shared<Subscribe>(session_);
    subscription_->module_change_subscribe(model_.c_str(),
                                           std::make_shared<SysrepoCallback>(),
                                           0, 0, SR_SUBSCR_DEFAULT);
  }

  void displayChanges() {
    // Every 1s, display changes.
    while (true) {
      {
        std::lock_guard<std::mutex> _(MUTEX);
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
      std::this_thread::sleep_for(1s);
    }
  }

  std::string const model_ = "model";
  S_Connection connection_;
  S_Session session_;
  S_Subscribe subscription_;
};

int main() {
  SysrepoClient client;
  client.displayChanges();
}
