#pragma once

#include <boost/beast/core/detail/base64.hpp>
#include <stdexcept>



//#define AVUTIL_STATIC // 在包含 FFmpeg 头文件前定义

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/version.hpp> // 确认 Boost 版本
#include <iostream>
#include <locale.h>
#include <mutex>
#include <string>
#include <thread>

#include <boost/asio/buffer.hpp>
#include <opus.h>
#include <stdexcept>
#include <vector>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

class encode_util
{
	public:
		std::string pcma_to_pcm_base64(const uint8_t *input_data, int input_size, int sample_rate);
		std::string to_base64(std::vector<uint8_t> pcm_buffer);
		std::string encode_base64(const unsigned char *opus_data, size_t data_size);
		std::string base64_encode_v1(const uint8_t *data, size_t len);
		std::string base64_encode(const std::vector<uint8_t> &input, bool with_padding);
		static std::vector<uint8_t> base64_decode(const std::string &input);
};
