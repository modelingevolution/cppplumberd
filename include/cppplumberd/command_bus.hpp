
#include <memory>
#include <string>
#include <boost/signals2.hpp>
using namespace std;
using namespace std::chrono;
using namespace boost::signals2;
namespace cppplumberd {

	class CommandBus {
	public:

		inline CommandBus(std::unique_ptr<ProtoReqRspClientHandler> clientHandler)
			: _handler(std::move(clientHandler)) {
			if (!_handler) {
				throw std::invalid_argument("ClientHandler cannot be null");
			}
		}

		template<typename TCommand>
		inline void Send(const TCommand& cmd) {
			_handler->Send<TCommand>(cmd);
		}

		template<typename TMessage, unsigned int MessageId>
		inline void RegisterMessage() {
			_handler->RegisterRequest<TMessage, MessageId>();
		}

		template<typename TError, unsigned int MessageId>
		inline void RegisterError() {
			_handler->RegisterError<TError, MessageId>();
		}

		inline void Start(const std::string& endpoint) {
			_handler->Start(endpoint);
		}

		inline void Start() {
			_handler->Start();
		}

	private:
		std::unique_ptr<ProtoReqRspClientHandler> _handler;
	};

}