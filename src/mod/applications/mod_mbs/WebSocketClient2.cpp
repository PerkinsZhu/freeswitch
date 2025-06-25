#include "WebSocketClient2.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <openssl/opensslv.h>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <openssl/bio.h>
#include <openssl/hmac.h>
#include <sstream>
#include <string>

using namespace std;
using namespace std::chrono;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;

using tcp = asio::ip::tcp;

class WebSocketClient2
{
	public:
	WebSocketClient2(const std::string &host, const std::string &port, ssl::context &ctx)
		: strand_(ioc.get_executor()), resolver_(ioc), ws_(ioc, ctx), timer_(ioc), host_(host), port_(port)
	{
		io_thread_ = std::thread(&WebSocketClient2::run_io_context, this);

		// 异步解析域名
		resolver_.async_resolve(
			host, port,
			boost::asio::bind_executor(strand_, [this, host](const boost::system::error_code &ec,
															 tcp::resolver::results_type results) {
				if (ec) {
					std::cerr << "解析失败: " << ec.message() << std::endl;
					return;
				}
				// 异步连接
				asio::async_connect(
					ws_.next_layer().next_layer(), results,

					boost::asio::bind_executor(strand_, [this, host](const boost::system::error_code &ec,
																	 const tcp::endpoint &) {
						if (ec) {
							std::cerr << "连接失败: " << ec.message() << std::endl;
							return;
						}

						// 设置WebSocket控制帧回调（处理Ping/Pong/Close）
						ws_.control_callback([this](websocket::frame_type type, beast::string_view payload) {
							this->on_control_frame(type, payload);
						});

						// 设置 SNI（关键步骤）
						if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
							boost::system::error_code ssl_ec{static_cast<int>(::ERR_get_error()),
															 boost::asio::error::get_ssl_category()};
							std::cerr << "SNI setup error: " << ssl_ec.message() << std::endl;
							return;
						}
						ws_.next_layer().async_handshake(
							ssl::stream_base::client,
							boost::asio::bind_executor(strand_, [this, host](const boost::system::error_code &ec) {
								if (ec) {
									std::cerr << "async_handshake 连接失败: " << ec.message() << std::endl;
									return;
								}

								std::string requrl = get_hand_shake();
								// 异步 WebSocket 握手
								ws_.async_handshake(
									host, requrl,

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

	std::string get_hand_shake()
	{
		int64_t timestamp =
			std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
				.count();

		std::string uuid_str = encode_util::generate_uuid_v4();

		std::string url = generate_auth_url("234", "234",
											"23423", 301005, uuid_str);
		
		


		std::cout << " 请求Url:" << url << std::endl;
		return url;
	}

	// 工具函数：URL 编码（简化实现，支持基本字符）
	std::string url_encode(const std::string &str)
	{
		std::ostringstream escaped;
		for (char c : str) {
			if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				escaped << c;
			} else {
				escaped << '%' << std::hex << std::uppercase << int(unsigned char(c));
			}
		}
		return escaped.str();
	}

	// HMAC-SHA1 + Base64 主函数
	std::string hmac_sha1_base64(const std::string &secret_key, const std::string &message)
	{
		// HMAC-SHA1 计算
		unsigned char digest[EVP_MAX_MD_SIZE];
		unsigned int digest_len = 0;

		HMAC(EVP_sha1(), secret_key.c_str(), secret_key.length(),
			 reinterpret_cast<const unsigned char *>(message.c_str()), message.length(), digest, &digest_len);

		if (digest_len == 0) { throw std::runtime_error("HMAC-SHA1 calculation failed"); }

		encode_util encode_util;
		// Base64 编码
		// return encode_util.base64_encode_v2(digest, digest_len);
		return encode_util::encode_base64(digest, digest_len);
	}

	// 生成签名
	string generate_signature(const string &api_secret, const std::string &signature_origin)
	{
		std::cout << "OpenSSL Version: " << OPENSSL_VERSION_TEXT << std::endl;

		for (char c : api_secret) { printf("%02x", (unsigned char)c); }

		printf("=============");
		for (char c : signature_origin) { printf("%02x", (unsigned char)c); }

		// 计算HMAC-SHA256
		unsigned char hmac_result[EVP_MAX_MD_SIZE];
		unsigned int hmac_len;
		HMAC(EVP_sha1(), api_secret.c_str(), api_secret.length(), (const unsigned char *)signature_origin.c_str(),signature_origin.length(), hmac_result, &hmac_len);


		std::cout << "C++ HMAC Bytes (Decimal): ";
		for (int i = 0; i < hmac_len; i++) {
			std::cout << (int)(unsigned char)hmac_result[i] << " "; // 强制转换为 unsigned char
		}
		std::cout << std::endl;



		// 生成 HMAC 后输出十六进制
		std::string hex;
		for (int i = 0; i < hmac_len; i++) {
			char buf[3];
			sprintf(buf, "%02x", hmac_result[i]);
			hex += buf;
		}
		std::cout << " HEX: "<< hex << std::endl; // 示例输出：a3d9f7e7b4d5e6c1b8a7f1d2e3c4b5a6

		// Base64编码
		string signature_sha(hmac_result, hmac_result + hmac_len);
		return encode_util::base64_encode(signature_sha);
	}


	
	 std::string hmac_sha1_2_str(const std::string &data, const std::string &key)
	{
		std::vector<unsigned char> digest(SHA_DIGEST_LENGTH);
		unsigned int digest_len;

		// 计算 HMAC
		HMAC(EVP_sha1(), key.c_str(), key.length(), reinterpret_cast<const unsigned char *>(data.c_str()),
			 data.length(), digest.data(), &digest_len);

		digest.resize(digest_len); // 调整到实际长度

				
		encode_util encode_util;
		// Base64 编码
		std::vector<uint8_t> dst = digest; // 直接赋值，无需转换

		 return encode_util.to_base64(dst);
	}
		// 计算 HMAC-SHA1 并返回二进制结果
	std::vector<unsigned char> hmac_sha1(const std::string &data, const std::string &key)
	{
		std::vector<unsigned char> digest(SHA_DIGEST_LENGTH);
		unsigned int digest_len;

		// 计算 HMAC
		HMAC(EVP_sha1(), key.c_str(), key.length(), reinterpret_cast<const unsigned char *>(data.c_str()),
			 data.length(), digest.data(), &digest_len);

		digest.resize(digest_len); // 调整到实际长度
		return digest;
	}

	// 将二进制数据转换为十六进制字符串
	std::string to_hex(const std::vector<unsigned char> &data)
	{
		static const char *hex_chars = "0123456789abcdef";
		std::string result;
		result.reserve(data.size() * 2);

		for (unsigned char c : data) {
			result.push_back(hex_chars[(c >> 4) & 0xF]);
			result.push_back(hex_chars[c & 0xF]);
		}

		return result;
	}
	// 生成鉴权 URL
	std::string generate_auth_url(const std::string &appid, const std::string &secret_id, const std::string &secret_key,
								  int voice_type, const std::string &voice_id)
	{

		// 基础参数
		int64_t timestamp =
			std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
				.count();
		int64_t expired = timestamp + 3600; // 有效期 1 小时

		// 参数列表（除 signature 外）
		std::vector<std::pair<std::string, std::string>> params = {
			{"SecretId", secret_id},
			{"Timestamp", std::to_string(timestamp)},
			{"Expired", std::to_string(expired)},
			{"VoiceType", std::to_string(voice_type)},
			{"SampleRate", "16000"},
			{"Codec", "pcm"},
			{"End", "0"},
			{"VoiceId", voice_id},
			{"Volume", "0"} // 可选参数
		};

		// 按字典序排序参数
		std::sort(params.begin(), params.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

		// 拼接签名原文
		std::stringstream ss;
		ss << "tts.cloud.tencent.com/vc_stream/" << appid << "?";
		for (size_t i = 0; i < params.size(); ++i) {
			ss << params[i].first << "=" << params[i].second;
			if (i != params.size() - 1) ss << "&";
		}
		std::string sign_origin = ss.str();

		std::cout << "sign_origin: " << sign_origin << endl;
		std::cout << "secret_key: " << secret_key << endl;
		// 生成签名


		// sign_origin = "ZHUPINGJING";
		//std::string signature = generate_signature(sign_origin, secret_key);
		//auto digest = hmac_sha1(sign_origin, secret_key);
		// std::string signature = to_hex(digest);

		std::string signature = hmac_sha1_2_str(sign_origin, secret_key);



		std::cout << "signature: " << signature << std::endl;
		signature = url_encode(signature); // 必须 URL 编码
		std::cout << "url_encode signature: " << signature << std::endl;

		// 拼接最终 URL
		std::stringstream final_url;
		final_url << "/vc_stream/" << appid << "?";
		for (const auto &param : params) { final_url << param.first << "=" << url_encode(param.second) << "&"; }
		final_url << "Signature=" << signature;

		std::cout << "final_url signature: " << final_url.str() << std::endl;

		return final_url.str();
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

		ws_.next_layer().next_layer().close(); // 关闭旧连接

		resolver_.async_resolve(
			host_, port_,
			boost::asio::bind_executor(strand_, [this](const boost::system::error_code &ec,
													   tcp::resolver::results_type results) {
				if (ec) {
					std::cerr << "解析失败: " << ec.message() << std::endl;
					return;
				}
				// 异步连接
				asio::async_connect(
					ws_.next_layer().next_layer(), results,

					boost::asio::bind_executor(strand_, [this](const boost::system::error_code &ec,
															   const tcp::endpoint &) {
						if (ec) {
							std::cerr << "连接失败: " << ec.message() << std::endl;
							return;
						}

						// 设置WebSocket控制帧回调（处理Ping/Pong/Close）
						ws_.control_callback([this](websocket::frame_type type, beast::string_view payload) {
							this->on_control_frame(type, payload);
						});

						// 设置 SNI（关键步骤）
						if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
							boost::system::error_code ssl_ec{static_cast<int>(::ERR_get_error()),
															 boost::asio::error::get_ssl_category()};
							std::cerr << "SNI setup error: " << ssl_ec.message() << std::endl;
							return;
						}
						ws_.next_layer().async_handshake(
							ssl::stream_base::client,
							boost::asio::bind_executor(strand_, [this](const boost::system::error_code &ec) {
								if (ec) {
									std::cerr << "async_handshake 连接失败: " << ec.message() << std::endl;
									return;
								}

								std::string requrl = get_hand_shake();
								// 异步 WebSocket 握手
								ws_.async_handshake(
									host_, requrl,
									boost::asio::bind_executor(strand_, [this](const boost::system::error_code &ec) {
										if (ec) {
											std::cerr << "握手失败: " << ec.message() << std::endl;
											return;
										}
										std::cout << "WebSocket 连接成功!" << std::endl;

										start_read();
										// start_ping();
										// start_timer(); // 启动定时器
									}));
							}));
					}));
			}));
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

						   std::string response = beast::buffers_to_string(this->read_buffer_.data());
						   std::cout << "收到消息: " << response << "\n";

						   std::cout << "Type of value: " << typeid(response).name() << std::endl;

						   std::string value = response.c_str();
						   std::cout << "c_str value: " << value << std::endl;

						   std::string value_v1 = encode_util::remove_leading_nulls(response);

						   // 去除前缀 'h'
						   if (!value_v1.empty() && value_v1[0] == 'p') { value_v1 = value_v1.substr(1); }

						   // 调用消息回调
						   if (on_message_) on_message_(value_v1);

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
	// websocket::stream<tcp::socket> ws_;
	websocket::stream<ssl::stream<tcp::socket>> ws_;

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
