#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <typeindex>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
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
			
			class Subscription : public ISubscription
			{
				SubscriptionManager* _parent;
				string _streamName;
				shared_ptr<IEventDispatcher> handler;

			public:
				IEventDispatcher& Dispatcher() const { return *handler; }
				string StreamName() const { return _streamName; }
				Subscription(SubscriptionManager* eventStore, const string& streamName, const shared_ptr<IEventDispatcher>& handler)
					: _parent(eventStore), _streamName(streamName), handler(handler) {
				}
				void Unsubscribe() override
				{
					_parent->Unsubscribe(this);
				}
			};
			unordered_multimap<string, shared_ptr<ProtoPublishHandler>> _publishedStreams;
			unordered_multimap<string, Subscription*> _localSubscribers;
			EventStore* _eventStore;
			void Unsubscribe(const Subscription* subscription)
			{
				auto range = _localSubscribers.equal_range(subscription->StreamName());
				for (auto it = range.first; it != range.second; ++it) {
					if (it->second == subscription) {
						_localSubscribers.erase(it);
						break;
					}
				}
			}
		public:
			
			SubscriptionManager(EventStore* parent) : _eventStore(parent) {}
			unique_ptr<ISubscription> Subscribe(const string& streamName, const shared_ptr<IEventDispatcher>& handler)
			{
				auto subscription = new Subscription(this, streamName, handler);
				_localSubscribers.insert({ streamName, subscription });
				return unique_ptr<ISubscription>(subscription);
			}

			void AddStream(const string& streamName, const shared_ptr<ProtoPublishHandler>& channel)
			{
				_publishedStreams.insert({ streamName, channel });
			}
			template<typename TEvent> // pushes events to local ISubscriptionManager 
			void Publish(const string& streamName, const TEvent& evt)
			{
				auto range = _localSubscribers.equal_range(streamName);
				for (auto &it = range.first; it != range.second; ++it) 
				{
					Metadata metadata(streamName, system_clock::now());
					unsigned int messageId = _eventStore->_serializer->GetMessageId<TEvent>();

					// Just pass the pointer to the const event
					// Using const_cast because the interface expects a non-const pointer
					// but we're not actually modifying the event
					MessagePtr ptr = const_cast<TEvent*>(&evt);
					
					it->second->Dispatcher().Handle(metadata, messageId, ptr);
					
				}

				auto range2 = _publishedStreams.equal_range(streamName);
				
				for (auto& it = range2.first; it != range2.second; ++it)
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