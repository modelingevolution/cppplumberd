#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

class NggTest : public ::testing::Test {
protected:
    const char* url = "ipc:///tmp/nng-pubsub-test";
    nng_socket pub;
    nng_socket sub;

    // Set up resources before each test
    void SetUp() override {
        // Create the publisher socket
        ASSERT_EQ(0, nng_pub0_open(&pub)) << "Failed to open publisher socket";

        // Listen on the specified URL
        ASSERT_EQ(0, nng_listen(pub, url, NULL, 0)) << "Failed to listen on publisher socket";

        // Create a subscriber socket
        ASSERT_EQ(0, nng_sub0_open(&sub)) << "Failed to open subscriber socket";

        // Subscribe to everything (empty topic)
        ASSERT_EQ(0, nng_setopt(sub, NNG_OPT_SUB_SUBSCRIBE, "", 0)) << "Failed to set subscription";

        // Connect subscriber to the publisher
        ASSERT_EQ(0, nng_dial(sub, url, NULL, 0)) << "Failed to connect to publisher";

        // Allow time for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean up resources after each test
    void TearDown() override {
        nng_close(pub);
        nng_close(sub);
        // Allow sockets to fully close
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Function to send message in a thread
    static void sendMessage(nng_socket socket, const char* message) {
        int rv = nng_send(socket, (void*)message, strlen(message) + 1, 0);
        EXPECT_EQ(0, rv) << "Failed to send message: " << nng_strerror(rv);
    }

    // Function to receive message in a thread
    static void receiveMessage(nng_socket socket, char** received_msg, size_t* msg_size, bool* success) {
        int rv = nng_recv(socket, received_msg, msg_size, NNG_FLAG_ALLOC);
        *success = (rv == 0);
        if (!*success) {
            std::cerr << "Failed to receive message: " << nng_strerror(rv) << std::endl;
        }
    }
};

// Basic pub-sub test with proper sequence
TEST_F(NggTest, BasicPubSubTest) {
    char* received_msg = nullptr;
    size_t msg_size = 0;
    bool receive_success = false;

    // Start receiver thread first
    std::thread recv_thread([this, &received_msg, &msg_size, &receive_success]() {
        receiveMessage(sub, &received_msg, &msg_size, &receive_success);
        });

    // Small delay to ensure receiver is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Start sender thread
    std::thread send_thread([this]() {
        sendMessage(pub, "hello");
        });

    // Wait for both threads to complete
    send_thread.join();
    recv_thread.join();

    // Verify the received message
    ASSERT_TRUE(receive_success) << "Failed to receive message";
    ASSERT_NE(nullptr, received_msg) << "No message received";
    EXPECT_STREQ("hello", received_msg) << "Received unexpected message";

    // Free the message buffer
    if (received_msg != nullptr) {
        nng_free(received_msg, msg_size);
    }
}

// Test multiple messages
TEST_F(NggTest, MultipleMessagesTest) {
    const int message_count = 5;
    const char* test_messages[message_count] = {
        "message1", "message2", "message3", "message4", "message5"
    };

    for (int i = 0; i < message_count; i++) {
        char* received_msg = nullptr;
        size_t msg_size = 0;
        bool receive_success = false;

        // Start receiver thread
        std::thread recv_thread([this, &received_msg, &msg_size, &receive_success]() {
            receiveMessage(sub, &received_msg, &msg_size, &receive_success);
            });

        // Small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Send message
        std::thread send_thread([this, i, &test_messages]() {
            sendMessage(pub, test_messages[i]);
            });

        // Wait for threads to complete
        send_thread.join();
        recv_thread.join();

        // Verify message
        ASSERT_TRUE(receive_success) << "Failed to receive message " << (i + 1);
        ASSERT_NE(nullptr, received_msg) << "No message received for message " << (i + 1);
        EXPECT_STREQ(test_messages[i], received_msg)
            << "Message " << (i + 1) << " content doesn't match";

        // Free the message buffer
        if (received_msg != nullptr) {
            nng_free(received_msg, msg_size);
        }

        // Small delay between test iterations
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
