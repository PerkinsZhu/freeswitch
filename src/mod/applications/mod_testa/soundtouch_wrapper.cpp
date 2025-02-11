#include <SoundTouch.h>
#include <switch.h>
#include <switch.h>
#include <stdlib.h>
#include <string.h>

// 定义 C 风格的结构体，用于封装 SoundTouch 对象
typedef struct {
	soundtouch::SoundTouch *st;
} SoundTouchWrapper;

// 创建 SoundTouch 对象的 C 风格函数
SoundTouchWrapper *createSoundTouch()
{
	SoundTouchWrapper *wrapper = new SoundTouchWrapper;
	wrapper->st = new soundtouch::SoundTouch();
	return wrapper;
}

// 销毁 SoundTouch 对象的 C 风格函数
void destroySoundTouch(SoundTouchWrapper *wrapper)
{
	if (wrapper) {
		delete wrapper->st;
		delete wrapper;
	}
}

// 其他需要的包装函数可以根据 SoundTouch 库的功能添加



// 音频处理回调函数
static switch_bool_t pitch_shift_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_frame_t *frame = NULL;
	soundtouch::SoundTouch::SoundTouch *soundTouch = (soundtouch::SoundTouch *)user_data;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		// 初始化 SoundTouch
		soundTouch->setSampleRate(8000);  // 设置采样率
		soundTouch->setChannels(1);		  // 设置声道数
		soundTouch->setPitchSemiTones(4); // 设置音调变化（例如提高 4 个半音）
		break;

	case SWITCH_ABC_TYPE_READ:
	case SWITCH_ABC_TYPE_WRITE:
		// 获取音频帧
		frame = switch_core_media_bug_get_frame(bug);
		if (frame) {
			// 处理音频数据
			soundTouch->putSamples((const SAMPLETYPE *)frame->data, frame->datalen / 2);
			uint samples_processed = soundTouch->receiveSamples((SAMPLETYPE *)frame->data, frame->datalen / 2);
			frame->datalen = samples_processed * 2; // 更新帧长度
		}
		break;

	case SWITCH_ABC_TYPE_CLOSE:
		// 清理 SoundTouch
		delete soundTouch;
		break;

	default:
		break;
	}

	return SWITCH_TRUE;
}

// 变声处理函数
void pitch_shift(float *input, float *output, int num_samples, int sample_rate)
{
	soundtouch::SoundTouch st;
	st.setPitch(1.2); // 提高音高，实现男声变女声
	st.setSampleRate(sample_rate);
	st.setChannels(1); // 单声道
	st.putSamples(input, num_samples);
	st.receiveSamples(output, num_samples);
}

