#include "WebSocketClient.h"

WebSocketClient::WebSocketClient( const std::string &host, const std::string &port,
								 const std::string &target)
	: resolver_(ioc_), host_(std::move(host)), port_(std::move(port)), target_(target), timer_(ioc_)
{
	reset_stream();
	io_thread_ = std::thread(&WebSocketClient::run_io_context, this);
}

WebSocketClient::~WebSocketClient()
{
	// 关闭连接并停止IO上下文
	if (ws_->is_open()) {
		ws_->async_close(websocket::close_code::normal, [](beast::error_code ec) {});
	}
	ioc_.stop();
	if (io_thread_.joinable()) { io_thread_.join(); }
}

// 设置回调函数
void WebSocketClient::set_on_message(std::function<void(const std::string &)> callback) { on_message_ = callback; }

void WebSocketClient::set_on_connect(std::function<void()> callback) { on_connect_ = callback; }

void WebSocketClient::set_on_error(std::function<void(const std::string &)> callback) { on_error_ = callback; }

// 启动连接
void WebSocketClient::start()
{
	running_ = true;
	connect();
}

// 停止并关闭连接
void WebSocketClient::stop()
{
	running_ = false;
	close();
}

// 发送消息
void WebSocketClient::send(const std::string &message)
{
	if (!connected_) {
		if (on_error_) on_error_("Not connected");
		return;
	}

	// 在 I/O 上下文中执行发送
	net::post(ws_->get_executor(), [self = shared_from_this(), message]() {
		self->ws_->async_write(net::buffer(message), [self](error_code ec, std::size_t) {
			if (ec) { self->handle_error("Send failed", ec); }
		});
	});
}

// 连接到服务器
void WebSocketClient::connect()
{
	if (!running_) return;

	// 解析服务器地址
	resolver_.async_resolve(host_, port_,
							[self = shared_from_this()](error_code ec, tcp::resolver::results_type results) {
								self->on_resolve(ec, results);
							});
}

// 处理地址解析结果
void WebSocketClient::on_resolve(error_code ec, tcp::resolver::results_type results)
{
	if (ec) {
		handle_error("Resolve failed", ec);
		return;
	}

	// 连接到服务器

	beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
	beast::get_lowest_layer(*ws_).async_connect(
		results, [self = shared_from_this()](error_code ec, tcp::endpoint) { self->on_connect(ec); });
}

// 处理连接结果
void WebSocketClient::on_connect(error_code ec)
{
	if (ec) {
		handle_error("Connect failed", ec);
		return;
	}

	     // 设置WebSocket控制帧回调（处理Ping/Pong/Close）
	ws_->control_callback([self = shared_from_this()](websocket::frame_type type, beast::string_view payload) {
		self->on_control_frame(type, payload);
	});


	// 执行 WebSocket 握手
	//beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
	//ws_->set_option(websocket::stream_base::decorator(
	//	[](websocket::request_type &req) { req.set(http::field::user_agent, "Boost.Beast WebSocket Client"); }));

	//websocket::stream_base::timeout opt{
	//	std::chrono::seconds(300),	   // 握手超时  秒
	//	std::chrono::seconds(1000000), // 空闲超时  秒
	//	true						   // keep ping
	//};
	//ws_->set_option(opt);

	// ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

	std::cout << "===== is opend :" << ws_->is_open() << std::endl;
	ws_->async_handshake(host_, target_, [self = shared_from_this()](error_code ec) { self->on_handshake(ec); });
}

// 处理握手结果
void WebSocketClient::on_handshake(error_code ec)
{
	if (ec) {
		handle_error("Handshake failed", ec);
		return;
	}

	// 重置重连状态
	reconnect_attempts_ = 0;
	current_reconnect_delay_ = initial_reconnect_delay_;
	connected_ = true;

	if (on_connect_) on_connect_();

	send_ping();
	// 开始读取消息
	read();
}

// 读取消息
void WebSocketClient::read()
{
	if (!running_) return;

	ws_->async_read(buffer_, [self = shared_from_this()](error_code ec, std::size_t bytes_transferred) {
		self->on_read(ec, bytes_transferred);
	});
}

// 处理读取结果
void WebSocketClient::on_read(error_code ec, std::size_t)
{
	if (ec) {
		handle_error("读数据异常.Read failed", ec);
		return;
	}

	// 提取消息
	read_msg_.assign(beast::buffers_to_string(buffer_.data()));
	buffer_.consume(buffer_.size());

	// 调用消息回调
	if (on_message_) on_message_(read_msg_);

	// 继续读取下一条消息
	read();
}

void WebSocketClient::reset_stream()
{
	tcp_stream_ = std::make_unique<beast::tcp_stream>(ioc_);
	ws_ = std::make_unique<websocket::stream<beast::tcp_stream &>>(*tcp_stream_);
}

// 关闭连接
void WebSocketClient::close()
{
	if (!connected_) return;

	// ws_->async_close(websocket::close_code::normal, [self = shared_from_this()](error_code ec) {
	//	self->on_close(ec);
	//
	//});

	error_code ec;
	ws_->close(websocket::close_code::normal, ec);
	beast::get_lowest_layer(*ws_).socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	beast::get_lowest_layer(*ws_).socket().close(ec);
	//reset_stream();
}

// 处理关闭结果
void WebSocketClient::on_close(error_code ec)
{
	connected_ = false;
	if (ec && ec != websocket::error::closed) {
		if (on_error_) on_error_("Close failed: " + ec.message());
	}
}

// 处理错误
void WebSocketClient::handle_error(const std::string &context, error_code ec)
{
	if (!running_) return;

	connected_ = false;

	std::string error_msg = context + ":  错误消息: " + ec.message() + "  错误码: " + std::to_string(ec.value());
	if (on_error_) on_error_(error_msg);

	/*if (ec == websocket::error::closed || ec == beast::errc::connection_reset || ec == beast::errc::broken_pipe) {
		schedule_reconnect();
	}*/

	schedule_reconnect();
}

// 安排重连
void WebSocketClient::schedule_reconnect()
{
	std::cout << "触发重连调度" << std::endl;
	if (!running_) return;
	//重连之前彻底关闭
	close();

	// 实现指数退避算法
	if (reconnect_attempts_ < max_reconnect_attempts_) {
		++reconnect_attempts_;
		if (reconnect_attempts_ > 1) { current_reconnect_delay_ = current_reconnect_delay_ * 2; }

		if (on_error_) {
			on_error_("Reconnecting in " + std::to_string(current_reconnect_delay_.count()) + " seconds (attempt " +
					  std::to_string(reconnect_attempts_) + "/" + std::to_string(max_reconnect_attempts_) + ")");
		}

		timer_.expires_after(current_reconnect_delay_);
		timer_.async_wait([self = shared_from_this()](error_code ec) {
			if (!ec) { self->connect(); }
		});
	} else {
		if (on_error_) on_error_("Max reconnect attempts reached");
	}
}

void WebSocketClient::send_ping()
{

	std::cerr << "send Ping ……………… \n";
	ws_->async_ping(beast::websocket::ping_data{}, [](boost::beast::error_code ec) {
		if (ec) {
			std::cerr << "Ping failed: " << ec.message() << "\n";
			return;
		}
	});

	timer_.expires_after(std::chrono::seconds(20));
	timer_.async_wait([self = shared_from_this()](error_code ec) {
		if (!ec) { self->send_ping(); }
	});
}



    // 处理控制帧（Ping/Pong/Close）
void WebSocketClient::on_control_frame(websocket::frame_type type, beast::string_view payload)
{
	switch (type) {
	case websocket::frame_type::ping:
		std::cout << "收到Ping，数据: " << payload << std::endl;
		// 注意：Beast默认自动回复Pong，此处仅打印信息
		break;
	case websocket::frame_type::pong:
		std::cout << "收到Pong，数据: " << payload << std::endl;
		break;
	case websocket::frame_type::close:
		std::cout << "收到关闭帧" << std::endl;
		break;
	}
}

void WebSocketClient::run_io_context()
{
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc_.get_executor());
	ioc_.run();
}

//
// int main2222()
//{
//	try {
//		net::io_context ioc;
//
//		// 创建 WebSocket 客户端
//		auto client = std::make_shared<WebSocketClient>(ioc, "127.0.0.1", "8887");
//
//		// 设置回调函数
//		client->set_on_connect([]() { std::cout << "Connected to server" << std::endl; });
//
//		client->set_on_message([](const std::string &msg) { std::cout << "Received: " << msg << std::endl; });
//
//		client->set_on_error([](const std::string &error) { std::cerr << "Error: " << error << std::endl; });
//
//		// 启动客户端
//		client->start();
//
//		// 发送测试消息
//		std::thread([client]() {
//			// 等待连接建立
//			std::this_thread::sleep_for(std::chrono::seconds(2));
//
//			// 发送消息
//			client->send("{\"name\":\"Hello, WebSocket!\"}");
//
//			// 模拟网络中断
//			std::this_thread::sleep_for(std::chrono::seconds(10));
//			std::cout << "Simulating network disconnection..." << std::endl;
//
//			// 再次发送消息（应该触发重连）
//			std::this_thread::sleep_for(std::chrono::seconds(5));
//			client->send("{\"name\":\"Testing reconnection!\"}");
//		}).detach();
//
//		// 运行 I/O 上下文
//		ioc.run();
//	} catch (std::exception const &e) {
//		std::cerr << "Exception: " << e.what() << std::endl;
//		return 1;
//	}
//
//	return 0;
//}
