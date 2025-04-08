#pragma once

#include <memory>
#include <functional>
#include <string>
#include "cppplumberd/proto_req_rsp_srv_handler.hpp"
#include "cppplumberd/fault_exception.hpp"

namespace cppplumberd {


    class Metadata
    {
	private:
		std::string _stream_id;
		time_point<system_clock> _created;

    public:
		Metadata() = default;
		Metadata(const std::string& stream_id)
			: _stream_id(stream_id){
		}

		Metadata(const string& string, time_point<system_clock> created) : _stream_id(string), _created(created) {
		}
		
		const std::string& StreamId() const { return _stream_id; }
		time_point<system_clock> Created() const {return _created;		}
    };

	class ICommandHandlerBase
	{
	public:
		virtual ~ICommandHandlerBase() = default;
	};
    template<typename TCommand>
    class ICommandHandler : public ICommandHandlerBase {
    public:
        virtual void Handle(const std::string& stream_id, const TCommand& cmd) = 0;
        
    };
    template<typename TEvent>
    class IEventHandler {
    public:
        virtual void Handle(const Metadata& metadata, const TEvent& evt) = 0;
        virtual ~IEventHandler() = default;
    };

	class IEventDispatcher
	{
	public:
		virtual void Handle(const Metadata&, unsigned int messageId, MessagePtr msg) = 0;
		virtual ~IEventDispatcher() = default;
	};
	class ISubscription
	{
	public:
		virtual void Unsubscribe() = 0;
		virtual ~ISubscription() = default;
	};
	class ISubscriptionManager
	{
	public:
		


		virtual unique_ptr<ISubscription> Subscribe(const string& streamName, const shared_ptr<IEventDispatcher>& handler) = 0;

		virtual ~ISubscriptionManager() = default;
	};
	
}