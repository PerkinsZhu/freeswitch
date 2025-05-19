#include "encode_util.h"




std::string encode_util::to_base64(std::vector<uint8_t> pcm_buffer)
{
	// 转换为 Base64
	std::string base64_str;
	base64_str.resize(boost::beast::detail::base64::encoded_size(pcm_buffer.size()));
	base64_str.resize(boost::beast::detail::base64::encode(&base64_str[0], pcm_buffer.data(), pcm_buffer.size()));
	return base64_str;
}



// 将 unsigned char* 数据转换为 Base64 字符串
std::string encode_util::encode_base64(const unsigned char *opus_data, size_t data_size)
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




std::string encode_util::base64_encode_v1(const uint8_t *data, size_t len)
{
	std::string result;
	result.resize(boost::beast::detail::base64::encoded_size(len));

	char *out_ptr = &result[0];
	const char *in_ptr = reinterpret_cast<const char *>(data);

	// 直接获取编码后的字节数（Boost 1.87 返回 size_t）
	size_t encoded_len = boost::beast::detail::base64::encode(out_ptr, // 输出缓冲区
													   in_ptr,	// 输入数据
													   len		// 输入数据长度
	);

	result.resize(encoded_len); // 使用 size_t 调整字符串长度
	return result;
}

/**
 * @brief 将二进制数据编码为 Base64 字符串
 * @param input 输入二进制数据（字节数组）
 * @param with_padding 是否包含 '=' 填充符（默认true，符合标准）
 * @return Base64 编码后的字符串
 */
std::string encode_util::base64_encode(const std::vector<uint8_t> &input, bool with_padding = true)
{
	try {
		// 计算编码后的长度
		const size_t encoded_len = boost::beast::detail::base64::encoded_size(input.size());

		// 分配输出缓冲区
		std::string output;
		output.resize(encoded_len);

		// 执行编码
		const size_t actual_len = boost::beast::detail::base64::encode(  &output[0], input.data(), input.size());

		// 调整输出长度（可选去除填充）
		if (!with_padding) {
			size_t pad_pos = output.find('=');
			if (pad_pos != std::string::npos) { output.resize(pad_pos); }
		} else {
			output.resize(actual_len); // 确保包含填充
		}

		return output;
	} catch (const std::exception &e) {
		throw std::runtime_error("Base64 encode error: " + std::string(e.what()));
	}
}


/**
 * @brief 将 Base64 字符串解码为二进制数据
 * @param input Base64 编码的字符串
 * @return 解码后的二进制数据（字节数组）
 */
 std::vector<uint8_t> encode_util::base64_decode(const std::string &input)
{
	try {
		// 计算解码后的最大可能长度
		const size_t max_decoded_len = boost::beast::detail::base64::decoded_size(input.size());

		// 分配输出缓冲区
		std::vector<uint8_t> output(max_decoded_len);

		// 执行解码
		const auto result = boost::beast::detail::base64::decode(output.data(), input.data(), input.size());

		// 调整实际解码长度
		output.resize(result.first);

		return output;
	} catch (const std::exception &e) {
		throw std::runtime_error("Base64 decode error: " + std::string(e.what()));
	}
}


