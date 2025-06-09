#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using error_code = boost::beast::error_code;

class WebSocketClient : public std::enable_shared_from_this<WebSocketClient>
{
	private:
	std::string host_;
	std::string port_;
	std::string target_;

	boost::asio::io_context ioc_;
	// asio::io_context ioc_;

	std::unique_ptr<beast::tcp_stream> tcp_stream_;
	std::unique_ptr<websocket::stream<beast::tcp_stream &>> ws_;

	std::thread io_thread_;

	tcp::resolver resolver_;
	std::atomic<bool> connected_{false};
	std::atomic<bool> running_{false};
	net::steady_timer timer_;
	beast::flat_buffer buffer_;
	std::string read_msg_;
	std::function<void(const std::string &)> on_message_;
	std::function<void()> on_connect_;
	std::function<void(const std::string &)> on_error_;
	int reconnect_attempts_{0};
	const int max_reconnect_attempts_{5};
	const std::chrono::seconds initial_reconnect_delay_{1};
	std::chrono::seconds current_reconnect_delay_{initial_reconnect_delay_};

	public:
	WebSocketClient();
	WebSocketClient(const std::string &host, const std::string &port, const std::string &target);
	~WebSocketClient();

	void set_on_message(std::function<void(const std::string &)> callback);

	void set_on_connect(std::function<void()> callback);

	void set_on_error(std::function<void(const std::string &)> callback);

	// 启动连接
	void start();
	// 停止并关闭连接
	void stop();
	// 发送消息
	void send(const std::string &message);

	private:
	// 连接到服务器
	void connect();

	// 处理地址解析结果
	void on_resolve(error_code ec, tcp::resolver::results_type results);

	// 处理连接结果
	void on_connect(error_code ec);

	// 处理握手结果
	void on_handshake(error_code ec);

	// 读取消息
	void read();

	// 处理读取结果
	void on_read(error_code ec, std::size_t);

	// 关闭连接
	void close();

	// 处理关闭结果
	void on_close(error_code ec);

	// 处理错误
	void handle_error(const std::string &context, error_code ec);

	// 安排重连
	void schedule_reconnect();

	void reset_stream();

	void send_ping();
	void on_control_frame(websocket::frame_type type, beast::string_view payload);
	void run_io_context();
};