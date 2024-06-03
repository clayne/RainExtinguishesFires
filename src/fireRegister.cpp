#include "fireRegister.h"

namespace {
	/*
	* Checks if given vector contains given element.
	* @param a_vec Pointer to the vector to check.
	* @param a_element Element to check for.
	* @return True if the element exists, false otherwise.
	*/
	template <typename T>
	bool VectorContainsElement(std::vector<T>* a_vec, T a_element) {
		return (std::find(a_vec->begin(), a_vec->end(), a_element) != a_vec->end());
	}
}

namespace CachedData {
	void Fires::SetLookupLight(double a_newValue) {
		if (a_newValue < 10.0) a_newValue = 10.0;
		if (a_newValue > 1000.0) a_newValue = 1000.0;
		this->lookupLightRadius = a_newValue;
	}

	void Fires::SetLookupSmoke(double a_newValue) {
		if (a_newValue < 10.0) a_newValue = 10.0;
		if (a_newValue > 1000.0) a_newValue = 1000.0;
		this->lookupSmokeRadius = a_newValue;
	}

	void Fires::SetReferenceLookupRadius(double a_newValue) {
		if (a_newValue < 10.0) a_newValue = 10.0;
		if (a_newValue > 1000.0) a_newValue = 1000.0;
		this->lookupREferenceRadius = a_newValue;
	}

	void Fires::SetRequiredOffTime(double a_newValue) {
		if (a_newValue < 0.5) a_newValue = 0.5;
		if (a_newValue > 10.0) a_newValue = 10.0;
		this->requiredOffTime = a_newValue;
	}

	void Fires::SetFireLookupRadius(double a_newValue) {
		if (a_newValue < 10.0) a_newValue = 10.0;
		if (a_newValue > 1000.0) a_newValue = 1000.0;
		this->fireLookupRadius = a_newValue;
	}

	const FireData* Fires::GetFireData(RE::TESBoundObject* a_form)
	{
		return nullptr;
	}

	void Fires::RegisterPair(RE::TESBoundObject* a_litForm, FireData fireData) {
		//Note, this should NEVER happen, both are checked. But sanity check.
		if (!a_litForm || !fireData.offVersion) return;
		_loggerInfo("Registered new fire to extinguish. Data:");
		_loggerInfo("    >Lit Version: {}", _debugEDID(a_litForm).empty() ? std::to_string(a_litForm->formID) : _debugEDID(a_litForm));
		_loggerInfo("    >Off Version: {}", _debugEDID(fireData.offVersion).empty() ? std::to_string(fireData.offVersion->formID) : _debugEDID(fireData.offVersion));
		_loggerInfo("    >Light lookup radius: {}", fireData.lightLookupRadius > 0.0 ? fireData.lightLookupRadius : 0.0);
		_loggerInfo("    >Smoke lookup radius: {}", fireData.smokeLookupRadius > 0.0 ? fireData.smokeLookupRadius : 0.0);
		_loggerInfo("    >Dyndolod fire: {}", fireData.dyndolodFire ? _debugEDID(fireData.dyndolodVersion) : "Not a DynDOLOD fire.");

		this->fireMap[a_litForm] = fireData;
	}
	void Fires::RegisterSmokeObject(RE::TESBoundObject* a_smokeObject) {
		if (VectorContainsElement<RE::TESBoundObject*>(&this->smokeVector, a_smokeObject)) return;
		this->smokeVector.push_back(a_smokeObject);

		_loggerInfo("Registered new smoke object: ", _debugEDID(a_smokeObject).empty() ? std::to_string(a_smokeObject->formID) : _debugEDID(a_smokeObject));
	}
}