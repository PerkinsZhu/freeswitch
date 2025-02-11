#include <stdio.h>


// 声明包装函数
typedef struct SoundTouchWrapper SoundTouchWrapper;
SoundTouchWrapper *createSoundTouch();
void destroySoundTouch(SoundTouchWrapper *wrapper);

int main()
{
	SoundTouchWrapper *wrapper = createSoundTouch();
	// 使用 wrapper 进行相关操作
	destroySoundTouch(wrapper);
	return 0;
}