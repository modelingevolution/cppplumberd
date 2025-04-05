1. App specs:

Have a 2 elements: fakevideosrc and fakevideosink
The first one, fakevideosrc, have a property called bufnumber = 100;
The second one, fakevideosink, have a property called processed = 0;

The app shall create a wireing infrastructure between cppplumberd and it's elements.
There shall be 2 executables: app-server and app-client;


So the test shall be:

app-server:
- Create a fakevideosrc element with property bufnumber = 100;
- Create a fakevideosink element with property processed = 0;
- Creates CommandHandler to handle the command CreateTopicSubscription (topic-name, set\<IPropertyInfo\>);
- Creates CommandHandler that handles SetProperty command (element-name, property-name, value);
- Creates property-monitoring-service that uses IEventPublisher (cppplumberd) to publish events.
- Creates TopicProcessor, IEventHandler, that subscribes for PropertyChanged events and publishes them on topic-stream.