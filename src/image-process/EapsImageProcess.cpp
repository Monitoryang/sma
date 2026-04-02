#include "EapsImageProcess.h"

namespace eap {
	namespace sma {

		ImageProcessPtr ImageProcess::createInstance()
		{
			return ImageProcessPtr(new ImageProcess());
		}

		ImageProcess::ImageProcess()
		{

		}

		ImageProcess::~ImageProcess()
		{

		}

	}
}