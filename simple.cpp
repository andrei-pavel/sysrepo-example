#include <sstream>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

using namespace sysrepo;
using namespace std::chrono_literals;

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

    if (event == SR_EV_UPDATE) {
      f(session);
    }

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
  }
};

struct SysrepoClient {
  SysrepoClient() {
    connection_ = std::make_shared<Connection>();
    session_ = std::make_shared<Session>(connection_, SR_DS_RUNNING);
    subscription_ = std::make_shared<Subscribe>(session_);
    subscription_->module_change_subscribe(
        model_.c_str(), std::make_shared<SysrepoCallback>(), nullptr, nullptr,
        0, SR_SUBSCR_UPDATE);
  }

  std::string const model_ = "model";
  S_Connection connection_;
  S_Session session_;
  S_Subscribe subscription_;
};

int main() {
  SysrepoClient client;
  while (true) {
    std::this_thread::sleep_for(1s);
  }

  return 0;
}
