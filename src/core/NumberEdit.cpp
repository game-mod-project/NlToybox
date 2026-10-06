#include "NumberEdit.hpp"

bool NlCore::StepNumberEdit(NumberEdit& Edit, bool Typed, double Value, bool Left, bool Active, double& Out)
{
	if (Typed)
	{
		Edit.Has = true;
		Edit.Value = Value;
	}
	if (Left)
	{
		const bool apply = Edit.Has;
		if (apply)
			Out = Edit.Value;
		Edit = NumberEdit();
		return apply;
	}
	if (!Active)
		Edit = NumberEdit();
	return false;
}
