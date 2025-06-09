#include <boost/beast/core/detail/base64.hpp>
#include <string>
#include <switch.h>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

// Opus 编码器配置
const int OPUS_SAMPLE_RATE = 8000;	// Opus 标准采样率
const int OPUS_CHANNELS = 1;		// 单声道
const int OPUS_FRAME_SIZE = 160;	// 20ms 帧大小（48000Hz * 0.02s）


// Opus 编码器封装类
class OpusEncoder
{
	private:
	AVCodecContext *codec_ctx = nullptr;
	SwrContext *swr_ctx = nullptr;
	std::vector<uint8_t> pcm_buffer;

	public:
	explicit OpusEncoder(int input_sample_rate, int input_channels)
	{
		// 初始化 Opus 编码器
		const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
		codec_ctx = avcodec_alloc_context3(codec);

		codec_ctx->bit_rate = 8000; // 64 kbps
		codec_ctx->sample_rate = OPUS_SAMPLE_RATE;
		codec_ctx->channel_layout = AV_CH_LAYOUT_MONO;
		codec_ctx->channels = OPUS_CHANNELS;
		codec_ctx->frame_size = OPUS_FRAME_SIZE;
		codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // 允许实验性编解码器
		codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;					   // 关键修改


		if (avcodec_open2(codec_ctx, codec, nullptr) < 0) { throw std::runtime_error("Failed to open Opus encoder"); }

		// 初始化重采样器（如果需要）
		if (input_sample_rate != OPUS_SAMPLE_RATE || input_channels != OPUS_CHANNELS) {
			swr_ctx = swr_alloc_set_opts(nullptr,
										 av_get_default_channel_layout(OPUS_CHANNELS), // 输出布局
										 AV_SAMPLE_FMT_FLTP,							   // 输出格式
										 OPUS_SAMPLE_RATE,
										 av_get_default_channel_layout(input_channels), // 输入布局
										 AV_SAMPLE_FMT_FLTP,								// 输入格式
										 input_sample_rate, 0, nullptr);
			if (!swr_ctx || swr_init(swr_ctx) < 0) { throw std::runtime_error("Failed to initialize resampler"); }
		}
	}

	~OpusEncoder()
	{
		if (codec_ctx) avcodec_free_context(&codec_ctx);
		if (swr_ctx) swr_free(&swr_ctx);
	}

	std::vector<uint8_t> encode(const uint8_t *pcm_data, int pcm_samples)
	{
		std::vector<uint8_t> opus_packets;

		// 重采样（如果需要）
		const uint8_t *src_data = pcm_data;
		if (swr_ctx) {
			std::vector<uint8_t> resampled_data(pcm_samples * 2); // 16-bit samples
			uint8_t *dst_data = resampled_data.data();
			int converted_samples = swr_convert(swr_ctx, &dst_data, pcm_samples, &src_data, pcm_samples);
			if (converted_samples < 0) { throw std::runtime_error("Resampling failed"); }
			src_data = dst_data;
		}

		// 分割为 Opus 帧
		int samples_remaining = pcm_samples;
		while (samples_remaining >= OPUS_FRAME_SIZE) {
			AVFrame *frame = av_frame_alloc();
			frame->nb_samples = OPUS_FRAME_SIZE;
			frame->format = AV_SAMPLE_FMT_FLTP;
			frame->channel_layout = AV_CH_LAYOUT_MONO;
			av_frame_get_buffer(frame, 0);

			memcpy(frame->data[0], src_data, OPUS_FRAME_SIZE * sizeof(int16_t));
			src_data += OPUS_FRAME_SIZE * sizeof(int16_t);
			samples_remaining -= OPUS_FRAME_SIZE;

			// 发送帧到编码器
			if (avcodec_send_frame(codec_ctx, frame) < 0) {
				av_frame_free(&frame);
				throw std::runtime_error("Failed to send frame to encoder");
			}
			av_frame_free(&frame);

			// 接收编码后的包
			AVPacket *pkt = av_packet_alloc();
			while (avcodec_receive_packet(codec_ctx, pkt) >= 0) {
				opus_packets.insert(opus_packets.end(), pkt->data, pkt->data + pkt->size);
				av_packet_unref(pkt);
			}
			av_packet_free(&pkt);
		}

		return opus_packets;
	}
};
