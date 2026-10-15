#pragma once
#include "B2BaseClass.h"
class WorldProperties : public B2BaseClass
{
	public:

	~WorldProperties( );

	protected :

		DDWORD EngineMessageFn(DDWORD messageID, void *pData, DFLOAT lData) override;
};


