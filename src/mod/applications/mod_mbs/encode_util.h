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
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

class encode_util
{
	public:
	std::string pcma_to_pcm_base64(const uint8_t *input_data, int input_size, int sample_rate);
	std::string to_base64(std::vector<uint8_t> pcm_buffer);
	std::string base64_encode_v1(const uint8_t *data, size_t len);
	std::string base64_encode(const std::vector<uint8_t> &input, bool with_padding);
	static std::vector<uint8_t> base64_decode(const std::string &input);
	std::string base64_encode_v2(const unsigned char *data, size_t length);
	static std::string encode_util::generate_uuid_v4()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 15);

		std::stringstream ss;
		ss << std::hex;
		for (int i = 0; i < 32; i++) {
			if (i == 8 || i == 12 || i == 16 || i == 20) ss << "-";
			ss << dis(gen);
		}
		return ss.str();
	}

	// 将 unsigned char* 数据转换为 Base64 字符串
	static std::string encode_util::encode_base64(const unsigned char *opus_data, size_t data_size)
	{
		if (opus_data == nullptr || data_size == 0) { throw std::invalid_argument("Invalid input data"); }

		// 计算 Base64 编码后的长度（包含填充）
		const size_t encoded_size = boost::beast::detail::base64::encoded_size(data_size);

		// 创建足够大的缓冲区存储结果
		std::string base64_str;
		base64_str.resize(encoded_size);

		// 执行编码
		const size_t actual_size = boost::beast::detail::base64::encode(&base64_str[0], // 输出缓冲区
																		opus_data, // 输入数据（自动转换为 const void*）
																		data_size // 输入数据大小
		);

		// 调整字符串长度以匹配实际编码后的数据
		base64_str.resize(actual_size);

		return base64_str;
	}

	static std::string remove_leading_nulls(std::string str)
	{
		// 找到第一个非空字符的位置
		size_t first_non_null = str.find_first_not_of('\0');

		// 如果存在非空字符，截取子串；否则返回空字符串
		if (first_non_null != std::string::npos) {
			return str.substr(first_non_null);
		} else {
			return "";
		}
	}

	// Base64编码函数

	static std::string base64_encode(const std::string &input)
	{
		BIO *bio, *b64;
		BUF_MEM *bufferPtr = nullptr;

		b64 = BIO_new(BIO_f_base64());
		bio = BIO_new(BIO_s_mem());
		bio = BIO_push(b64, bio);
		BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
		BIO_write(bio, input.c_str(), input.length());
		BIO_flush(bio);
		BIO_get_mem_ptr(bio, &bufferPtr);

		std::string result(bufferPtr->data, bufferPtr->length);
		BIO_free_all(bio);
		return result;
	}
};
