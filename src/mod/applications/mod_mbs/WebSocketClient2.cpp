#include "WebSocketClient2.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

using namespace std;
using namespace std::chrono;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;

class WebSocketClient2
{
	public:
	WebSocketClient2(const std::string &host, const std::string &port)
		: strand_(ioc.get_executor()), resolver_(ioc), ws_(ioc), timer_(ioc), host_(host), port_(port)
	{
		io_thread_ = std::thread(&WebSocketClient2::run_io_context, this);

		// 异步解析域名
		resolver_.async_resolve(
			host, port,
			boost::asio::bind_executor(
				strand_, [this](const boost::system::error_code &ec, tcp::resolver::results_type results) {
					if (ec) {
						std::cerr << "解析失败: " << ec.message() << std::endl;
						return;
					}
					// 异步连接
					asio::async_connect(
						ws_.next_layer(), results,

						boost::asio::bind_executor(
							strand_, [this](const boost::system::error_code &ec, const tcp::endpoint &) {
								if (ec) {
									std::cerr << "连接失败: " << ec.message() << std::endl;
									return;
								}

								// 设置WebSocket控制帧回调（处理Ping/Pong/Close）
								ws_.control_callback([this](websocket::frame_type type, beast::string_view payload) {
									this->on_control_frame(type, payload);
								});

								// 异步 WebSocket 握手
								ws_.async_handshake(
									host_ + ":" + port_, "/ws",

									boost::asio::bind_executor(strand_, [this](const boost::system::error_code &ec) {
										if (ec) {
											std::cerr << "握手失败: " << ec.message() << std::endl;
											return;
										}
										std::cout << "WebSocket 连接成功!" << std::endl;

										start_read();
										start_ping();
										// start_timer(); // 启动定时器
									}));
							}));
				}));
	}
	~WebSocketClient2()
	{
		// 关闭连接并停止IO上下文
		if (ws_.is_open()) {
			ws_.async_close(websocket::close_code::normal, [](beast::error_code ec) {});
		}
		ioc.stop();
		if (io_thread_.joinable()) { io_thread_.join(); }
	}

	// 处理控制帧（Ping/Pong/Close）
	void on_control_frame(websocket::frame_type type, beast::string_view payload)
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

	// 设置回调函数
	void set_on_message(std::function<void(const std::string &)> callback) { on_message_ = callback; }

	int print_time()
	{
		// 获取当前时间点（包含毫秒）
		auto now = system_clock::now();

		// 转换为 time_t（秒级精度）
		auto now_time_t = system_clock::to_time_t(now);

		// 转换为本地时间的 tm 结构体
		tm local_tm;
		localtime_s(&local_tm, &now_time_t); // Windows 线程安全

		// 提取毫秒部分
		auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

		// 格式化输出
		cout << put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count() << endl;
		return 0;
	}
	// 异步发送消息
	void send_message(const std::string &msg)
	{
		{
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::ostringstream oss;

			oss << " before write -> Thread: " << std::this_thread::get_id() << ")" << std::endl;

			std::cout << oss.str();
		}

		// 在 I/O 上下文中执行发送
		this->ws_.async_write(boost::asio::buffer(msg),
							  boost::asio::bind_executor(strand_,
														 [this, msg](boost::system::error_code ec, std::size_t) {
															 std::ostringstream oss1;
															 oss1 << " write -> Thread: " << std::this_thread::get_id()
																  << ")" << std::endl;
															 std::cout << oss1.str();
															 if (ec) {
																 std::cerr << "发送失败: " << ec.message() << std::endl;
																 // 尝试重新连接
																 reconnect();
																 return;
															 }
															 print_time();
															 std::cout << "消息已完成发送: "
																	   << "" << std::endl;
														 })

		);
	}

	private:
	// 启动定时器，20秒后触发
	void start_timer()
	{
		timer_.expires_after(asio::chrono::seconds(20));
		timer_.async_wait([this](const boost::system::error_code &ec) {
			if (ec) {
				if (ec != asio::error::operation_aborted) { std::cerr << "定时器错误: " << ec.message() << std::endl; }
				return;
			}
			// send_message("Hello from Client!"); // 发送消息
		});
	}

	// 重新连接逻辑
	void reconnect()
	{

		print_time();
		std::cerr << "重新连接…… " << std::endl;

		ws_.next_layer().close(); // 关闭旧连接
		//重新链接
		resolver_.async_resolve(
			host_, port_, [this](const boost::system::error_code &ec, tcp::resolver::results_type results) {
				if (ec) {
					std::cerr << "重新解析失败: " << ec.message() << std::endl;
					return;
				}
				asio::async_connect(
					ws_.next_layer(), results, [this](const boost::system::error_code &ec, const tcp::endpoint &) {
						if (ec) {
							std::cerr << "重新连接失败: " << ec.message() << std::endl;
							return;
						}
						ws_.async_handshake(host_ + ":" + port_, "/", [this](const boost::system::error_code &ec) {
							if (ec) {
								std::cerr << "重新握手失败: " << ec.message() << std::endl;
								return;
							}
							std::cout << "重新连接成功!" << std::endl;
							// start_timer(); // 重启定时器
							start_read();
						});
					});
			});
	}

	// 启动数据读取循环
	void start_read()
	{
		{

			std::lock_guard<std::mutex> lock(read_cout_mutex);
			std::ostringstream oss;

			oss << "beford read -> Thread: " << std::this_thread::get_id() << ")" << std::endl;
			std::cout << oss.str();
		}

		ws_.async_read(read_buffer_, boost::asio::bind_executor(strand_, [this](beast::error_code ec, size_t) {
						   std::ostringstream oss1;

						   oss1 << " read -> Thread: " << std::this_thread::get_id() << ")" << std::endl;
						   std::cout << oss1.str();

						   if (ec == websocket::error::closed) {
							   std::cout << "连接已关闭\n";
							   return;
						   }
						   if (ec) return this->fail(ec, "read");

						  // std::cout << "收到消息: " << beast::buffers_to_string(this->read_buffer_.data()) << "\n";

						   // 调用消息回调
						   if (on_message_) on_message_(beast::buffers_to_string(this->read_buffer_.data()));

						   this->read_buffer_.consume(this->read_buffer_.size());
						   this->start_read(); // 继续读取下一条消息
					   }));
	}

	// 定时发送 Ping（可选）
	void start_ping()
	{
		timer_.expires_after(asio::chrono::seconds(20));
		timer_.async_wait([this](beast::error_code ec) {
			if (ec) return;

			// 发送 Ping

			this->ws_.async_ping("client-ping", boost::asio::bind_executor(strand_,

																		   [this](beast::error_code ec) {
																			   std::cout << " ping -> Thread: "
																						 << std::this_thread::get_id()
																						 << ")" << std::endl;

																			   if (!ec) {
																				   std::cout << "已发送 Ping\n";
																				   this->start_ping(); // 重启定时器
																			   }
																		   }));
		});
	}

	// 错误处理
	void fail(beast::error_code ec, const char *what)
	{
		std::cerr << "[错误] " << what << ": " << ec.message() << "\n";
		ws_.async_close(websocket::close_code::normal, [](beast::error_code) {});
	}

	void run_io_context()
	{
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc.get_executor());
		ioc.run();
	}

	std::thread io_thread_;
	boost::asio::io_context ioc;
	beast::flat_buffer read_buffer_;
	tcp::resolver resolver_;
	websocket::stream<tcp::socket> ws_;
	asio::steady_timer timer_;
	std::string host_;
	std::string port_;
	asio::strand<asio::io_context::executor_type> strand_;
	std::mutex cout_mutex;
	std::mutex read_cout_mutex;

	std::function<void(const std::string &)> on_message_;
};

// int main()
//{
//	asio::io_context io;
//	WebSocketClient2 client(io, "127.0.0.1", "8887"); // 连接到公共测试服务器
//	io.run();											   // 主线程在此处阻塞，处理所有异步事件
//	return 0;
//}
