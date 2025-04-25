#include <switch.h>
//#include "WSClient.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <mutex>
#include <boost/version.hpp> // 确认 Boost 版本

#include <iostream>
#include <locale.h>
#include <string>


using namespace std;
namespace beast = boost::beast;			// from <boost/beast.hpp>
namespace http = beast::http;			// from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace asio = boost::asio;			// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;		// from <boost/asio/ip/tcp.hpp>
static std::thread io_thread;


SWITCH_BEGIN_EXTERN_C

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mbs_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_mbs_load);
SWITCH_MODULE_DEFINITION(mod_mbs, mod_mbs_load, mod_mbs_shutdown, NULL);

SWITCH_END_EXTERN_C

// 每个会话的私有数据（包含 WebSocket 连接）
struct SessionData {
	websocket::stream<tcp::socket> ws;
	asio::io_context &ioc;
	std::mutex write_mutex;

	SessionData(asio::io_context &ctx) : ioc(ctx), ws(ctx) {}
	~SessionData()
	{
		beast::error_code ec;
		ws.close(websocket::close_code::normal, ec);
	}
};
// 全局资源（使用 executor_work_guard 替代 io_context::work）
struct GlobalContext {
	asio::io_context ioc;
	using work_guard_type = asio::executor_work_guard<asio::io_context::executor_type>;
	std::unique_ptr<work_guard_type> work_guard;
	std::vector<std::thread> io_threads;

	GlobalContext()
	{
		work_guard = std::make_unique<work_guard_type>(ioc.get_executor());
		// 启动 4 个 IO 线程处理异步任务
		for (int i = 0; i < 4; ++i) {
			io_threads.emplace_back([this] { ioc.run(); });
		}
	}

	~GlobalContext()
	{
		work_guard.reset(); // 取消 work_guard，允许 ioc.run() 自然退出
		for (auto &t : io_threads) {
			if (t.joinable()) t.join();
		}
	}
};

static GlobalContext *g_ctx = nullptr;

switch_status_t res;
switch_api_interface_t *api_interface = NULL;
switch_application_interface_t *app_interface = NULL;



// 全局WebSocket客户端指针
//static WSClient *ws_client = nullptr;


websocket::stream<tcp::socket> *ws_;

static void run_io_context(asio::io_context &ioc) { ioc.run(); }

void receiveMessages(websocket::stream<tcp::socket> &ws)
{
	try {
		beast::flat_buffer buffer;
		boost::system::error_code ec;

		while (true) {
			// 接收服务器响应
			ws.read(buffer, ec);

			if (ec == websocket::error::closed) {
				std::cout << "Closed: " << ws.reason().reason << std::endl;
			} else if (ec) {
				std::cerr << "Error: " << ec.message() << std::endl;
				return;
			} else {

				std::string response = beast::buffers_to_string(buffer.data());

				std::cout << "Received: " << response << std::endl;
			}
			buffer.consume(buffer.size()); // 清空缓冲区
		}
	} catch (const std::exception &e) {
		std::cerr << "Receive error: " << e.what() << std::endl;
	}
}

	//连接websocket
websocket::stream<tcp::socket> connect_websocket(std::string IP, const char *port_s)
{

	asio::io_context ioc;
	tcp::resolver resolver{ioc};
	websocket::stream<tcp::socket> ws{ioc};
	ws_ = &ws;



		auto const address = asio::ip::make_address(IP);				  //服务器地址
		auto const port = static_cast<unsigned short>(std::atoi(port_s)); //服务器端口号
		tcp::endpoint endpoint{address, port};
		auto const results = resolver.resolve(endpoint);
		// 在我们从查找中获得的IP地址上建立连接
		asio::connect(ws.next_layer(), results.begin(), results.end());
		ws.set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
			req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
		}));
		std::cout << "==============The port is:" << port_s << std::endl;
		ws.handshake(IP, "/"); //发送握手消息
		
         //启动接收消息的线程
		std::thread receiveThread(receiveMessages, std::ref(ws));

		auto size = ws.write(asio::buffer("====123======"));

		return ws;

}



std::string getbody(short seq, short status, std::string audio) { 
	
	return "23232323";

}


std::string base64_encode(const uint8_t *data, size_t len)
{
	std::string result;
	result.resize(beast::detail::base64::encoded_size(len));

	char *out_ptr = &result[0];
	const char *in_ptr = reinterpret_cast<const char *>(data);

	// 直接获取编码后的字节数（Boost 1.87 返回 size_t）
	size_t encoded_len = beast::detail::base64::encode(out_ptr, // 输出缓冲区
													   in_ptr,	// 输入数据
													   len		// 输入数据长度
	);

	result.resize(encoded_len); // 使用 size_t 调整字符串长度
	return result;
}

// media bug回调函数
static switch_bool_t my_media_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);

	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = {0};
	frame.data = data;
	frame.buflen = sizeof(data);
	
	if (user_data == NULL) { return SWITCH_TRUE; }


	SessionData *session_data = static_cast<SessionData *>(user_data);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		//ws_client->connect("cn-huadong-1.xf-yun.com", "80");
		
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug initialized\n");
		break;

	case SWITCH_ABC_TYPE_READ:
		// 处理读取的音频数据

		if (switch_core_media_bug_read(bug, &frame, SWITCH_FALSE) != SWITCH_STATUS_FALSE) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "read frame: data_len=[%d], rate[%d], channels[%d], samples[%d] \n", frame.datalen,
							  frame.rate, frame.channels, frame.samples);

			  // 转换为 Base64
			const auto *data = reinterpret_cast<const uint8_t *>(frame.data);
			std::string encoded = base64_encode(data, frame.datalen);

			// 异步发送（确保数据生命周期）
			auto buffer = std::make_shared<std::string>(std::move(encoded));

			// 异步发送（线程安全）
			asio::post(g_ctx->ioc, [session_data, buffer] {
				beast::error_code ec;
				session_data->ws.write(asio::buffer(*buffer), ec);
				if (ec) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "发送失败: %s\n", ec.message().c_str());
				}
			});

		}
		break;

	case SWITCH_ABC_TYPE_WRITE:
		// 处理写入的音频数据
		break;

	case SWITCH_ABC_TYPE_CLOSE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug closed\n");
		// ws_->close(websocket::close_code::normal); // 关闭WebSocket连接
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		switch_frame_t *frame;
		int flag;
		//获取语音数据
		//frame = switch_core_media_bug_get_read_replace_frame(bug);

		
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

//测试媒体流获取
static void test_media_bug(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_status_t status;


	//connect_websocket("172.20.113.213", "8887");	

	 // 获取或创建会话私有数据
	switch_channel_t *channel = switch_core_session_get_channel(session);
	SessionData *session_data = static_cast<SessionData *>(switch_channel_get_private(channel, "mbs_data"));

	if (!session_data) {
		// 新建 WebSocket 连接
		session_data = new SessionData(g_ctx->ioc);
		try {
			tcp::resolver resolver(g_ctx->ioc);
			auto const results = resolver.resolve("172.20.113.213", "8887");
			asio::connect(session_data->ws.next_layer(), results.begin(), results.end());
			session_data->ws.handshake("172.20.113.213", "/ws");
		} catch (const std::exception &e) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WebSocket 连接失败: %s\n", e.what());
			delete session_data;
			return ;
		}


		
		status = switch_core_media_bug_add(session, "my_media_bug", NULL, my_media_bug_callback, session_data, 0,
										   SMBF_READ_STREAM | SWITCH_ABC_TYPE_READ_REPLACE | SMBF_NO_PAUSE, &bug);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add media bug!\n");
			return;
		}

		// 存储到通道私有数据
		switch_channel_set_private(channel, "mbs_data", session_data);
	}


	return ;

}



static void session_cleanup(switch_core_session_t* session) {
    switch_channel_t* channel = switch_core_session_get_channel(session);
    SessionData* session_data = static_cast<SessionData*>(switch_channel_get_private(channel, "mbs_data"));

    if (session_data) {
        switch_channel_set_private(channel, "mbs_data", nullptr);
        delete session_data; // 触发 WebSocket 关闭
    }
}


	//执行APP事件，该APP需要在 diaplan中 通过action 标签触发
SWITCH_STANDARD_APP(task_app_function)
{
	// task_app(session, data);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "===task_app_function start, session=%p, data=%s\n",
					  (void *)session, data);


	test_media_bug(session);

}


SWITCH_MODULE_LOAD_FUNCTION(mod_mbs_load)
{
	//switch_api_interface_t *api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	    // 初始化全局 IO 上下文和线程池
	g_ctx = new GlobalContext();

	SWITCH_ADD_APP(app_interface, "task_app", "task_app", "task_app", task_app_function, "NULL", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);


   

	return SWITCH_STATUS_SUCCESS;
}







SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mbs_shutdown)
{
	if (g_ctx) {
		delete g_ctx;
		g_ctx = nullptr;
	}
	return SWITCH_STATUS_SUCCESS;
}

