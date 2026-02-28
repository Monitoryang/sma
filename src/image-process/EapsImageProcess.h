#ifndef EAPS_IMAGE_PROCESS
#define EAPS_IMAGE_PROCESS

#include <memory>

namespace eap {
	namespace sma {
		class ImageProcess;
		using ImageProcessPtr = std::shared_ptr<ImageProcess>;

		class ImageProcess
		{
		public:
			static ImageProcessPtr createInstance();
			~ImageProcess();

		public://func


		private://func

		private://member

		private:
			ImageProcess();
			ImageProcess(ImageProcess& other) = delete;
			ImageProcess(ImageProcess&& other) = delete;
			ImageProcess& operator=(ImageProcess& other) = delete;
			ImageProcess& operator=(ImageProcess&& other) = delete;
		};
	}
}

#endif // !EAPS_IMAGE_PROCESS



