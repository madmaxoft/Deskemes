#include "PgSucceeded.hpp"
#include "ui_PgSucceeded.h"
#include "../NewDeviceWizard.hpp"





PgSucceeded::PgSucceeded(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgSucceeded)
{
	mUI->setupUi(this);
	setFinalPage(true);
}





PgSucceeded::~PgSucceeded()
{
	// Nothing explicit needed yet
}
