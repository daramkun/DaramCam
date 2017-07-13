#ifndef __DARAMCAM_MEDIAFOUNDATIONGENERATOR_INTERNAL_H__
#define __DARAMCAM_MEDIAFOUNDATIONGENERATOR_INTERNAL_H__

static unsigned __convertFrameTick ( unsigned frameTick )
{
	switch ( frameTick )
	{
	case DCMFVF_FRAMETICK_60FPS: return 60;
	case DCMFVF_FRAMETICK_30FPS: return 30;
	case DCMFVF_FRAMETICK_24FPS: return 24;
	default: return 0;
	}
}

#endif