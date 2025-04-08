#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <typeindex>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <boost/signals2.hpp>
using namespace std;
using namespace std::chrono;
using namespace boost::signals2;
namespace cppplumberd {

	class EventStore
	{
		class SubscriptionManager : ISubscriptionManager
		{
			unordered_map<string, shared_ptr<ProtoPublishHandler>> _publishedStreams;
			unordered_map<string, shared_ptr<IEventDispatcher>> _localSubscribers;
			EventStore* _eventStore;
		public:
			SubscriptionManager(EventStore* parent) : _eventStore(parent) {}
			unique_ptr<ISubscription> Subscribe(const string& streamName, const shared_ptr<IEventDispatcher>& handler)
			{
				_localSubscribers[streamName] = handler;
			}

			void AddStream(const string& streamName, const shared_ptr<ProtoPublishHandler>& channel)
			{
				_publishedStreams[streamName] = channel;
			}
			template<typename TEvent> // pushes events to local ISubscriptionManager 
			void Publish(const string& streamName, const TEvent& evt)
			{
				auto lit = _localSubscribers.find(streamName);
				if (lit != _localSubscribers.end())
				{
					
					MessagePtr ptr = const_cast<MessagePtr>(&evt);
					lit->second->Handle(Metadata(streamName), _eventStore->_serializer->GetMessageId<TEvent>(), ptr);
				}


				auto it = _publishedStreams.find(streamName);
				if (it != _publishedStreams.end())
					it->second->Publish(evt);


			}
		};
		unique_ptr<SubscriptionManager> _subscriptionManager;
		shared_ptr<MessageSerializer> _serializer;
		shared_ptr<ISocketFactory> _socketFactory;
	public:
		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage()
		{
			_serializer->RegisterMessage<TMessage, MessageId>();
		}

		EventStore()
		{
			_subscriptionManager = make_unique<SubscriptionManager>(this);
			_serializer = make_shared<MessageSerializer>();
		}
		EventStore(shared_ptr<ISocketFactory> socketFactory)
			: _socketFactory(socketFactory)
		{
			_subscriptionManager = make_unique<SubscriptionManager>(this);
			_serializer = make_shared<MessageSerializer>();
		}
		EventStore(shared_ptr<ISocketFactory> socketFactory, shared_ptr<MessageSerializer> serializer)
		{
			_socketFactory = socketFactory;
			_subscriptionManager = make_unique<SubscriptionManager>(this);
			_serializer = serializer;
		}

		virtual void CreateStream(const string& streamName)
		{
			auto h = make_shared<ProtoPublishHandler>(_socketFactory->CreatePublishSocket(streamName), _serializer);

			_subscriptionManager->AddStream(streamName, h);

			h->Start();
		}

		template<typename TEvent> // pushes events to local ISubscriptionManager and remote channels.
		void Publish(const string& streamName, const TEvent& evt)
		{
			_subscriptionManager->Publish(streamName, evt);
		}
	};
}