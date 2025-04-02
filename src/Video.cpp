#include "Video.h"

#if defined(_WIN32) && defined(WITH_SMALLPOT)
#include "PotDll.h"
#endif

Video::Video(Window* w)
{
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    smallpot_ = PotCreateFromWindow(w);
#endif
}

Video::~Video()
{
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    PotDestory(smallpot_);
#endif
}

int Video::playVideo(const std::string& filename)
{
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    return PotInputVideo(smallpot_, (char*)filename.c_str());
#endif
    return 0;
}
