#include "mod_mbs.h"
#include <switch.h>
//#include "OpusEncoder.cpp"
#include <functional>
#include <mutex>
#include <string>
#include <thread>

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

#define OPUS_MAX_PACKET_SIZE 4000

// 定义结构体保存数据和长度
class AudioBuffer
{
	public:
	uint8_t *data;
	size_t data_size;
	AudioBuffer() {}
};

// 每个会话的私有数据（包含 WebSocket 连接）
struct SessionData {
	int seq = 0;

	std::shared_ptr<WebSocketClient2> webSocketClient;

	// 音频数据队列（用于存放待发送和已接收的数据）
	switch_queue_t *audio_queue;
	// 时间戳同步锁
	switch_mutex_t *mutex;
	// 当前音频位置的时间戳（单位：采样数）
	uint32_t ts;

	SessionData()
	{
		try {
			// 创建 WebSocket 客户端
			webSocketClient = std::make_shared<WebSocketClient2>("127.0.0.1", "8887");

		} catch (const std::exception &e) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WebSocket 连接失败: %s\n", e.what());
			return;
		}
	}
	~SessionData() {}
};
switch_status_t res;
switch_api_interface_t *api_interface = NULL;
switch_application_interface_t *app_interface = NULL;

boolean is_stop = false; //标记程序是否要正常结束

websocket::stream<tcp::socket> *ws_;

// void receiveMessages(SessionData *session_data);

std::string getResponseAudio(std::string json_data)
{

	try {
		// 序列化为字符串
		boost::json::value jv = boost::json::parse(json_data);
		return jv.at("payload").at("input_audio").at("audio").get_string().c_str();

	} catch (std::exception &e) {
		std::cout << "========> exception : " << e.what() << std::endl;
		return "";
	}
}
AudioBuffer *vector_to_audio_buffer(const std::vector<uint8_t> &audio_data)
{
	AudioBuffer *buffer = new AudioBuffer();

	if (audio_data.empty()) {
		buffer->data = nullptr;
		buffer->data_size = 0;
		return buffer;
	}

	// 深拷贝数据到动态内存
	buffer->data_size = audio_data.size();
	buffer->data = new uint8_t[buffer->data_size];				   // 分配内存
	std::copy(audio_data.begin(), audio_data.end(), buffer->data); // 复制数据

	return buffer;
}

void free_audio_buffer(AudioBuffer &buffer)
{
	delete[] buffer.data; // 释放动态内存
	buffer.data = nullptr;
	buffer.data_size = 0;
}

static void on_message(std::string response, SessionData *session_data)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "websocket RECEIVE : %s \n", response.c_str());

	string audio = getResponseAudio(response);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "websocket audio : %s \n", audio.c_str());

	std::vector<uint8_t> audio_data = encode_util::base64_decode(audio);

	uint8_t buffer[SWITCH_RECOMMENDED_BUFFER_SIZE];

	AudioBuffer *audio_buffer = vector_to_audio_buffer(audio_data);

	switch_queue_push(session_data->audio_queue, audio_buffer);

	uint32_t queue_size = switch_queue_size(session_data->audio_queue);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "=== queue_size : %u\r\n", queue_size);
}

std::string getbody(short seq, short status, std::string audio)
{

	string json_data = R"({
        "payload": {
        "input_audio": {
            "sample_rate": 16000,
                "channels" : 1,
                "audio" : "",
                "encoding" : "opus",
                "bit_depth" : 16,
                "frame_size" : 320,
                "seq" : 0,
                "status" : 0
        }
    },
        "parameter": {
        "xvc": {
            "volume": 0,
                "result" : {
                "sample_rate": 16000,
                    "channels" : 1,
                    "encoding" : "opus",
                    "bit_depth" : 16,
                    "frame_size" : 320
            },
                "voiceName": "yifei",
                    "vocoder_mode" : 0,
                    "pitch" : 0,
                    "speed" : -500
        }
    },
        "header": {
        "app_id": "44b4c1ab",
            "status" : 0
    }
})";

	// 序列化为字符串
	boost::json::value jv = boost::json::parse(json_data);
	jv.at("payload").as_object().at("input_audio").as_object()["audio"] = audio;
	jv.at("payload").as_object().at("input_audio").as_object()["seq"] = seq;
	jv.at("payload").as_object().at("input_audio").as_object()["status"] = status;
	jv.at("header").as_object()["status"] = status;

	return boost::json::serialize(jv);
}

// media bug回调函数
static switch_bool_t my_media_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{

	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	const char *caller_id = switch_channel_get_variable(channel, "caller_id_name");
	const char *callee_id = switch_channel_get_variable(channel, "callee_id_name");
	const char *direction = switch_channel_get_variable(channel, "direction");

	 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,  "==== into my_media_bug_callback caller_id:[%s] --> called_id:[%s] direction:[%s]  type:[%d]\n", caller_id, callee_id, direction, type);

	if (user_data == NULL) {
		return SWITCH_TRUE; }

	switch_frame_t *frame_read;
	SessionData *session_data = static_cast<SessionData *>(user_data);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		SetConsoleOutputCP(CP_UTF8); //解决windows控制台输出中文乱码
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug initialized\n");
		break;
	case SWITCH_ABC_TYPE_READ: {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SWITCH_ABC_TYPE_READ\n");
		break;
	}

	case SWITCH_ABC_TYPE_READ_REPLACE: {
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SWITCH_ABC_TYPE_READ_REPLACE\n");
		frame_read = switch_core_media_bug_get_read_replace_frame(bug);

		if (!frame_read) { return SWITCH_TRUE; }

		//静音检测
		int flag = silk_VAD_Get((const short *)frame_read->data);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SILK VAD RESULT: [%d]\n",flag);

		if (flag) {
			encode_util encodeUtil;

			const uint8_t *pcm_data = reinterpret_cast<uint8_t *>(frame_read->data);
			string audio = encodeUtil.base64_encode_v1(pcm_data, frame_read->datalen);
			// 更新时间戳
			session_data->ts += frame_read->samples;

			int seq = session_data->seq;
			std::string sendBody = getbody(session_data->seq, seq == 0 ? 0 : 1, audio);
			session_data->seq = seq + 1;

			session_data->webSocketClient->send_message(sendBody);
		}

		uint32_t queue_size = switch_queue_size(session_data->audio_queue);

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "=== write queue_size : %u\r\n", queue_size);

		void *item;
		if (queue_size > 0 && switch_queue_pop(session_data->audio_queue, &item) == SWITCH_STATUS_SUCCESS) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "====replace data from queue ====: %s \r\n", item);
			AudioBuffer *audio_buffer = static_cast<AudioBuffer *>(item);

			// 覆盖原始音频帧
			memcpy(frame_read->data, audio_buffer->data, audio_buffer->data_size);
			frame_read->datalen = audio_buffer->data_size;

			// memset(frame_read->data, 0, frame_read->datalen);
		} else {
			// 无数据时填充静音（0x00或特定静音包）
			memset(frame_read->data, 0, frame_read->datalen);
		}
		// frame_read->datalen = 0;
		// memset(frame_read->data, 0, frame_read->datalen);

		break;
	}
	case SWITCH_ABC_TYPE_CLOSE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug closed\n");
		break;
	case SWITCH_ABC_TYPE_WRITE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " SWITCH_ABC_TYPE_WRITE \n");
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

//测试媒体流获取
void test_media_bug(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_status_t status;

	// 获取或创建会话私有数据
	switch_channel_t *channel = switch_core_session_get_channel(session);
	SessionData *session_data = static_cast<SessionData *>(switch_channel_get_private(channel, "mbs_data"));

	if (!session_data) {
		// 新建 WebSocket 连接
		session_data = new SessionData();

		session_data->ts = 0;

		// 创建音频队列和互斥锁
		switch_memory_pool_t *session_pool = switch_core_session_get_pool(session);

		switch_queue_create(&(session_data)->audio_queue, 1000, session_pool);
		switch_mutex_init(&(session_data)->mutex, SWITCH_MUTEX_NESTED, session_pool);

		session_data->webSocketClient->set_on_message(
			[session_data](const std::string response) { on_message(response, session_data); });

		// SMBF_WRITE_REPLACE | SMBF_READ_REPLACE | SWITCH_ABC_TYPE_READ | SWITCH_ABC_TYPE_WRITE
		//| SMBF_WRITE_STREAM | SMBF_WRITE_REPLACE
		status = switch_core_media_bug_add(session, "my_media_bug", NULL, my_media_bug_callback, session_data, 0,
										   SMBF_READ_REPLACE, &bug);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add media bug!\n");
			return;
		}

		// 存储到通道私有数据
		switch_channel_set_private(channel, "mbs_data", session_data);
	}

	return;
}

static void session_cleanup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	SessionData *session_data = static_cast<SessionData *>(switch_channel_get_private(channel, "mbs_data"));

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
	// switch_api_interface_t *api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "task_app", "task_app", "task_app", task_app_function, "NULL",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mbs_shutdown)
{

	is_stop = true;

	return SWITCH_STATUS_SUCCESS;
}
