#pragma once
#include<stdio.h>
#include <json/json.h>
#include <thread>

#include "common.hpp"
#include "utils.hpp"

extern "C" {
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include "3rd/utils.h"
}
#pragma comment(lib, "lib/jsoncpp.lib")
#pragma comment(lib, "./lib/librabbitmq.4.lib")
#pragma comment(lib, "ws2_32.lib")

#define SUMMARY_EVERY_US 1000000

namespace fits {
	typedef struct MQ {

		static MQ* getMQ(std::string mqHost,
			std::string mqUsername,
			std::string mqPassword,
			std::string mqQueueName,
			int port) {
			return instance ? instance : instance = new MQ(mqHost, mqUsername, mqPassword, mqQueueName, port);
		}
		static MQ* getMQ() {
			return instance ? instance : instance = new MQ();
		}
		/*void start() {
			amqp_thread = std::thread(run, conn);
		}*/
#if 0
		int init() {
			amqp_socket_t* socket = NULL;
			//amqp_connection_state_t conn;

			amqp_bytes_t queuename;
			conn = amqp_new_connection();

			socket = amqp_tcp_socket_new(conn);
			if (!socket) {
				throw ("creating TCP socket");
			}

			int status = amqp_socket_open(socket, mqHost.c_str(), port);
			if (status) {
				std::cout << amqp_error_string2(status) << "\n";
				throw ("opening TCP socket");
			}

			die_on_amqp_error(amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
				mqUsername, mqPassword),
				"Logging in");
			amqp_channel_open(conn, 1);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

			/*{
				amqp_queue_declare_ok_t* r = amqp_queue_declare(
					conn, 1, amqp_cstring_bytes("fits.core"), 0, 1, 0, 0, amqp_empty_table);
				die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue");
				queuename = amqp_bytes_malloc_dup(r->queue);
				if (queuename.bytes == NULL) {
					fprintf(stderr, "Out of memory while copying queue name");
					return -1;
				}
			}

			amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes("amq.fanout"),
				amqp_cstring_bytes("fits.core"), amqp_empty_table);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
			*/
#if 0  
			//消费信息
			amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0,
				amqp_empty_table);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Consuming");
#endif
			return 0;
		}
#endif
#if 1
		int init() {
			amqp_socket_t* socket;
			amqp_rpc_reply_t reply;
			int status, result = 0;

			do {
				conn = amqp_new_connection();
				socket = amqp_tcp_socket_new(conn);
				if (!socket)
				{
					printf("amqp new socket error\n");
					result = -1;
					break;
				}
				status = amqp_socket_open(socket, mqHost.c_str(), port);
				if (status)
				{
					printf("amqp open socket error\n");
					result = -2;
					break;
				}
				/*reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
					amqp_cstring_bytes(mqUsername.data()), amqp_cstring_bytes(mqPassword.data()));*/
				reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
					&mqUsername, &mqPassword);
				if (reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION)
				{
					printf("amqp login error\n");
					result = -3;
					break;
				}
				amqp_channel_open(conn, 1);
				reply = amqp_get_rpc_reply(conn);
				if (reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION)
				{
					printf("ConnectRabbitmq::amqp get rpc_reply error\n");
					result = -4;
					break;
				}
				amqp_queue_bind(conn, 1, amqp_cstring_bytes(mqQueueName.c_str()), amqp_cstring_bytes("amq.fanout"),
					amqp_cstring_bytes("fits.core"), amqp_empty_table);
				die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
			} while (false);

			return result;
		}
#endif
		void deinit() {
			amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
			amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
			amqp_destroy_connection(conn);

		}

		void send(char const* queue_name, std::string msg) {
			char message[2048];

			amqp_bytes_t message_bytes;

			//for (int i = 0; i < (int)sizeof(message); i++) {
			//	message[i] = '\0';
			//}
			memset(message, 0, sizeof(message));

			int len = msg.length();
			if (msg.length() > sizeof(message)) {
				Utils::log("msg too long [%s] %d\n", msg.data(), msg.length());
			}
			//for (int i = 0; msg[i] != '\0'; i++) {
			//	message[i] = msg[i];
			//	len = i;
			//}
			memcpy(message, msg.data(), std::min(sizeof(message), msg.length()));

			message_bytes.len = len;
			message_bytes.bytes = message;

			die_on_error(amqp_basic_publish(conn, 1, amqp_cstring_bytes("amq.fanout"),
				amqp_cstring_bytes(mqQueueName.c_str()), 0, 0, NULL,
				message_bytes),
				"Publishing");
		}

		void consume() {
			amqp_rpc_reply_t reply;
			//自动回复ACK
			amqp_basic_consume(conn, 1, amqp_cstring_bytes(mqQueueName.c_str()), amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
			reply = amqp_get_rpc_reply(conn);
			if (reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION)
			{
				printf("StartConsumer::amqp get rpc_reply error\n");
			}
			{
				amqp_rpc_reply_t res;
				amqp_envelope_t envelope;

				amqp_maybe_release_buffers(conn);

				res = amqp_consume_message(conn, &envelope, NULL, 0);

				/*if (AMQP_RESPONSE_NORMAL != res.reply_type) {
					break;
				}*/

				printf("Delivery %u, exchange %.*s routingkey %.*s\n",
					(unsigned)envelope.delivery_tag,
					(int)envelope.exchange.len, (char*)envelope.exchange.bytes,
					(int)envelope.routing_key.len, (char*)envelope.routing_key.bytes);

				if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
					printf("Content-type: %.*s\n",
						(int)envelope.message.properties.content_type.len,
						(char*)envelope.message.properties.content_type.bytes);
				}
				printf("----\n");

				char buf[1024];
				int len = envelope.message.body.len;
				for (int i = 0;i < len;i++) {
					buf[i] = ((unsigned char*)(envelope.message.body.bytes))[i];
				}
				std::string msg = buf;
				std::string key = "";
				std::string value = "";

				Json::CharReaderBuilder b;
				Json::CharReader* reader(b.newCharReader());
				JSONCPP_STRING errs;
				Json::Value jsonE;
				const char* msgToCh = msg.c_str();
				reader->parse(msgToCh, msgToCh + std::strlen(msgToCh), &jsonE, &errs);
				//amqp_dump(envelope.message.body.bytes, envelope.message.body.len);
			}
		}
#if 0
	private:
		static void run(amqp_connection_state_t conn) {
			uint64_t start_time = now_microseconds();
			int received = 0;
			int previous_received = 0;
			uint64_t previous_report_time = start_time;
			uint64_t next_summary_time = start_time + SUMMARY_EVERY_US;

			amqp_frame_t frame;

			uint64_t now;

			for (;;) {
				amqp_rpc_reply_t ret;
				amqp_envelope_t envelope;

				now = now_microseconds();
				if (now > next_summary_time) {
					int countOverInterval = received - previous_received;
					double intervalRate =
						countOverInterval / ((now - previous_report_time) / 1000000.0);
					printf("%d ms: Received %d - %d since last report (%d Hz)\n",
						(int)(now - start_time) / 1000, received, countOverInterval,
						(int)intervalRate);

					previous_received = received;
					previous_report_time = now;
					next_summary_time += SUMMARY_EVERY_US;
				}

				amqp_maybe_release_buffers(conn);
				ret = amqp_consume_message(conn, &envelope, NULL, 0);

				if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
					if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type &&
						AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
						if (AMQP_STATUS_OK != amqp_simple_wait_frame(conn, &frame)) {
							return;
						}

						if (AMQP_FRAME_METHOD == frame.frame_type) {
							switch (frame.payload.method.id) {
							case AMQP_BASIC_ACK_METHOD:
								/* if we've turned publisher confirms on, and we've published a
								 * message here is a message being confirmed.
								 */
								break;
							case AMQP_BASIC_RETURN_METHOD:
								/* if a published message couldn't be routed and the mandatory
								 * flag was set this is what would be returned. The message then
								 * needs to be read.
								 */
							{
								amqp_message_t message;
								ret = amqp_read_message(conn, frame.channel, &message, 0);
								std::cout << message.properties.message_id.bytes << "--------\n";
								std::cout << message.body.bytes << "--------\n";
								if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
									return;
								}

								amqp_destroy_message(&message);
							}

							break;

							case AMQP_CHANNEL_CLOSE_METHOD:
								/* a channel.close method happens when a channel exception occurs,
								 * this can happen by publishing to an exchange that doesn't exist
								 * for example.
								 *
								 * In this case you would need to open another channel redeclare
								 * any queues that were declared auto-delete, and restart any
								 * consumers that were attached to the previous channel.
								 */
								return;

							case AMQP_CONNECTION_CLOSE_METHOD:
								/* a connection.close method happens when a connection exception
								 * occurs, this can happen by trying to use a channel that isn't
								 * open for example.
								 *
								 * In this case the whole connection must be restarted.
								 */
								return;

							default:
								fprintf(stderr, "An unexpected method was received %u\n",
									frame.payload.method.id);
								return;
							}
						}
					}

				}
				else {
					amqp_destroy_envelope(&envelope);
				}

				received++;
			}
		}
# endif
	private:
		std::thread amqp_thread;
		amqp_connection_state_t conn;
		int port;
		std::string mqHost;
		std::string mqUsername;
		std::string mqPassword;
		std::string mqQueueName;
		MQ() {}
		MQ(std::string mqHost,
			std::string mqUsername,
			std::string mqPassword,
			std::string mqQueueName,
			int port) {
			this->mqHost = mqHost;
			this->mqUsername = mqUsername;
			this->mqPassword = mqPassword;
			this->mqQueueName = mqQueueName;
			this->port = port;
		}

		static MQ* instance;
	} *PMQ;
}