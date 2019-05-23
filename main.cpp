#include <sstream>
#include <string>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

using namespace sysrepo;
using namespace std::chrono_literals;

std::vector<S_Change> changes;

struct SysrepoCallback : Callback {
  void logChanges(S_Session const &session) {
    S_Iter_Change iterator(session->get_changes_iter("/model:config"));
    if (!iterator) {
      std::cerr << "no iterator" << std::endl;
      return;
    }

    while (true) {
      S_Change change;
      try {
        change = session->get_change_next(iterator);
      } catch (sysrepo_exception const &ex) {
        std::cerr << "get change iterator next failed: " << ex.what()
                  << std::endl;
        return;
      }
      if (!change) {
        // End of changes, not an error.
        std::cout << "end of changes" << std::endl;
        return;
      }
      changes.push_back(change);
    }
  }

  int module_change(S_Session session, const char * /* module_name */,
                    sr_notif_event_t event, void * /* private_ctx */) {
    std::ostringstream event_type;
    switch (event) {
    case SR_EV_VERIFY:
      event_type << "VERIFY";
      break;
    case SR_EV_APPLY:
      event_type << "APPLY";
      break;
    case SR_EV_ABORT:
      event_type << "ABORT";
      break;
    case SR_EV_ENABLED:
      event_type << "ENABLED";
      break;
    default:
      event_type << "UNKNOWN (" << event << ")";
      break;
    }
    std::cout << event_type.str() << std::endl;
    logChanges(session);
    return 0;
  }

  void module_install(const char* module_name,
                      const char* revision,
                      sr_module_state_t /*state*/,
                      void* /*private_ctx*/) {
    std::cout << module_name << " " << revision << std::endl;
  }
};

struct SysrepoClient {
  std::string model_ = "model";
  S_Connection connection_;
  S_Session session_;
  S_Subscribe subscription_;

  void init() {
    try {
      connection_.reset(
          new Connection("test-application", SR_CONN_DAEMON_REQUIRED));
    } catch (std::exception const &ex) {
      std::cerr << "cannot connect to sysrepo: " << ex.what() << std::endl;
      exit(1);
    }

    try {
      session_.reset(new Session(connection_, SR_DS_RUNNING));
    } catch (std::exception const &ex) {
      std::cerr << "cannot establish a session: " << ex.what() << std::endl;
      exit(2);
    }

    // Every 2s, display changes.
    std::thread([&]() mutable {
      while (true) {
        for (S_Change const &change : changes) {
          std::cout << "Old: " << change->old_val()->to_string() << std::endl;
          std::cout << "New: " << change->new_val()->to_string() << std::endl;
          std::cout << "Operation:: " << change->oper() << std::endl << std::endl;
        }
        std::this_thread::sleep_for(2s);
      }
    }).detach();
  }

  void subscribeConfig() {
    subscription_.reset(new Subscribe(session_));
    S_Callback cb(new SysrepoCallback());
    try {
      sr_subscr_options_t options = SR_SUBSCR_DEFAULT;
      subscription_->module_change_subscribe(model_.c_str(), cb, 0, 0, options);
      subscription_->module_install_subscribe(cb);
    } catch (std::exception const &ex) {
      std::cerr << "module change subscribe failed with: " << ex.what() << std::endl;
      exit(3);
    }
  }
};

int main() {
  SysrepoClient client;
  client.init();
  client.subscribeConfig();

  select(0, nullptr, nullptr, nullptr, nullptr);
  return 0;
}
