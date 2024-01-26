#include "eventDispenser.h"

namespace Events {
	bool IsFormInVector(RE::TESForm* a_form, std::vector<RE::TESForm*> a_vec) {
		if (!a_vec.empty()) {
			for (auto* form : a_vec) {
				if (form == a_form) return true;
			}
		}
		return false;
	}

	/**
	* Returns the closest reference with the provided type.
	* @param a_center The reference from which to search.
	* @param a_radius The radius within which to search.
	* @param a_type The type of reference to check. 31 is lights, 36 Moveable statics.
	*/
	RE::TESObjectREFR* GetNearestReferenceOfType(RE::TESObjectREFR* a_center, float a_radius, RE::FormType a_type, bool a_matchStoredObjects) {
		RE::TESObjectREFR* response = nullptr;
		bool found = false;
		float lastDistance = a_radius;
		auto refLocation = a_center->GetPosition();

		if (const auto TES = RE::TES::GetSingleton(); TES) {
		TES->ForEachReferenceInRange(a_center, a_radius, [&](RE::TESObjectREFR* a_ref) {
				const auto baseBound = a_ref ? a_ref->GetBaseObject() : nullptr;
				if (!baseBound) return RE::BSContainer::ForEachResult::kContinue;
				if (!baseBound->Is(a_type)) return RE::BSContainer::ForEachResult::kContinue;

				if (a_matchStoredObjects) {
					auto* baseForm = baseBound->As<RE::TESForm>();
					if (!baseForm) return RE::BSContainer::ForEachResult::kContinue;
					if (!CachedData::FireRegistry::GetSingleton()->IsValidObject(baseForm)) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
				}

				auto lightLocation = a_ref->GetPosition();
				float currentDistance = lightLocation.GetDistance(refLocation);
				if (currentDistance > lastDistance) return RE::BSContainer::ForEachResult::kContinue;

				found = true;
				response = a_ref;
				lastDistance = currentDistance;
				return RE::BSContainer::ForEachResult::kContinue;
				});
		}
		if (found) return response;
		return nullptr;
	}

	RE::TESObjectREFR* GetNearestMatchingDynDOLODFire(RE::TESObjectREFR* a_center, float a_radius, std::string a_name) {
		if (a_name.empty()) return nullptr;

		bool found = false;
		RE::TESObjectREFR* response = nullptr;
		if (const auto TES = RE::TES::GetSingleton(); TES) {
			auto centerLocation = a_center->data.location;
			TES->ForEachReferenceInRange(a_center, a_radius, [&](RE::TESObjectREFR* a_ref) {
				auto* baseBound = a_ref ? a_ref->GetBaseObject() : nullptr;
				if (!baseBound) return RE::BSContainer::ForEachResult::kContinue;

				if (clib_util::editorID::get_editorID(baseBound->As<RE::TESForm>()).contains(a_name)) {
					response = a_ref;
					found = true;
				}
				return RE::BSContainer::ForEachResult::kContinue;
				});
		}

		if (found) return response;
		return nullptr;
	}

	void Papyrus::Papyrus::AddWeatherChangeListener(const RE::TESForm* a_form, bool a_listen) {
		if (this->disable) return;

		if (a_listen) {
			this->weatherTransition.Register(a_form);
		}
		else {
			this->weatherTransition.Unregister(a_form);
		}
	}

	void Papyrus::Papyrus::AddInteriorExteriorListener(const RE::TESForm* a_form, bool a_listen) {
		if (this->disable) return;
		if (a_listen) {
			this->movedToExterior.Register(a_form);
		}
		else {
			this->movedToExterior.Unregister(a_form);
		}
	}

	bool Papyrus::IsRaining() {
		if (this->isRaining) {
			return true;
		}

		auto* skySingleton = RE::Sky::GetSingleton();
		if (!skySingleton) {
			return false;
		}

		if (!this->currentWeather) {
			if (!skySingleton->currentWeather) {
				return false;
			}
			this->currentWeather = skySingleton->currentWeather;
		}
		if (!this->currentWeather) {
			return false;
		}

		float currentWeatherPct = RE::Sky::GetSingleton()->currentWeatherPct;
		if (currentWeatherPct > 0.85f) {
			if (this->currentWeather->data.flags & RE::TESWeather::WeatherDataFlag::kRainy
				|| this->currentWeather->data.flags & RE::TESWeather::WeatherDataFlag::kSnow) {
				this->isRaining = true;
				return true;
			}
		}
		return false;
	}

	bool Papyrus::ManipulateFireRegistry(RE::TESObjectREFR* a_fire, bool a_add) {
		if (this->disable) return false;
		if (!a_fire) return false;

		auto fireBound = a_fire->GetBaseObject();
		auto* fireBase = fireBound ? fireBound->As<RE::TESForm>() : nullptr;
		if (!fireBase) return false;

		bool foundMatch = CachedData::FireRegistry::GetSingleton()->IsOffFire(fireBase);
		if (!foundMatch && !CachedData::FireRegistry::GetSingleton()->IsOnFire(fireBase)) return false;

		if (a_add) {
			if (this->frozenFiresRegister.contains(a_fire)) return false;
			this->frozenFiresRegister[a_fire] = true;
		}
		else {
			if (!this->frozenFiresRegister.contains(a_fire)) return false;
			this->frozenFiresRegister.erase(a_fire);
		}
		return true;
	}

	void Papyrus::RelightFire(RE::TESObjectREFR* a_litFire) {
		if (this->disable) return;
		if (this->frozenFiresRegister.contains(a_litFire)) return;

		auto baseBound = a_litFire->GetBaseObject();
		auto* baseForm = baseBound ? baseBound->As<RE::TESForm>() : nullptr;
		if (!baseForm) return;
		if (!CachedData::FireRegistry::GetSingleton()->IsOffFire(baseForm)) return;
		if (!this->ManipulateFireRegistry(a_litFire, true)) return;

		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* handlePolicy = vm->GetObjectHandlePolicy();
		if (!handlePolicy) {
			this->ManipulateFireRegistry(a_litFire, false);
			return;
		}

		RE::VMHandle handle = handlePolicy->GetHandleForObject(a_litFire->GetFormType(), a_litFire);
		if (!handle) {
			this->ManipulateFireRegistry(a_litFire, false);
			return;
		}

		for (auto& foundScript : vm->attachedScripts.find(handle)->second) {
			if (!foundScript) continue;
			if (!foundScript->GetTypeInfo()) continue;
			if (foundScript->GetTypeInfo()->GetName() != "REF_ObjectRefOffController"sv) continue;

			auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
			auto args = RE::MakeFunctionArguments();
			const RE::BSFixedString functionName = "Relight"sv;
			auto scriptObject = foundScript.get();
			auto object = RE::BSTSmartPointer<RE::BSScript::Object>(scriptObject);
			vm->DispatchMethodCall(object, functionName, args, callback);
			break;
		}
	}

	void Papyrus::SendWeatherChange(RE::TESWeather* a_weather) {
		if (this->disable) return;
		if (!a_weather) return;

		if (!currentWeather) {
			currentWeather = RE::Sky::GetSingleton()->lastWeather;
		}

		auto* playerCell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
		if (!playerCell) return;
		if (playerCell->IsInteriorCell()) return;

		bool bWasRaining = false;

		if (currentWeather) {
			if (currentWeather->data.flags & RE::TESWeather::WeatherDataFlag::kRainy
				|| currentWeather->data.flags & RE::TESWeather::WeatherDataFlag::kSnow) {
				bWasRaining = true;
			}
		}
		bool bIsRaining = false;

		if (a_weather->data.flags & RE::TESWeather::WeatherDataFlag::kRainy
			|| a_weather->data.flags & RE::TESWeather::WeatherDataFlag::kSnow) {
			bIsRaining = true;
		}

		float weatherTransitionTime = 25.0f;
		float weatherPct = RE::Sky::GetSingleton()->currentWeatherPct;

		if (!bWasRaining && bIsRaining) {
			float precipitation = (-0.2f) * a_weather->data.precipitationBeginFadeIn; //fadeBegin is int8_t
			float passedTime = weatherPct * weatherTransitionTime / 100.0f;
			float remainingTime = precipitation - passedTime;

			if (remainingTime < 1.0f) remainingTime = 1.0f;

			this->weatherTransition.QueueEvent(true, remainingTime);
			this->currentWeather = a_weather;
		}
		else if (bWasRaining && !bIsRaining) {
			float clearPrecipitation = (-0.2f) * a_weather->data.precipitationEndFadeOut;
			float passedTime = weatherPct * weatherTransitionTime / 100.0f;
			float remainingTime = clearPrecipitation - passedTime;

			if (remainingTime < 1.0f) remainingTime = 1.0f;

			this->weatherTransition.QueueEvent(false, remainingTime);
			this->currentWeather = a_weather;
		}
	}

	void Papyrus::SendPlayerChangedInteriorExterior(bool a_movedToExterior) {
		if (this->disable) return;
		this->movedToExterior.QueueEvent(a_movedToExterior);
	}

	void Papyrus::ExtinguishFire(RE::TESObjectREFR* a_fire, CachedData::FireData a_data) {
		if (this->disable) return;
		if (this->frozenFiresRegister.contains(a_fire)) return;
		auto* offForm = a_data.offVersion;
		if (!offForm) return;
		std::vector<RE::TESObjectREFR*> additionalExtinguishes = std::vector<RE::TESObjectREFR*>();

		auto* referenceExtraList = &a_fire->extraList;
		bool hasEnableChildren = referenceExtraList ? referenceExtraList->HasType(RE::ExtraDataType::kEnableStateChildren) : false;
		bool hasEnableParents = referenceExtraList ? referenceExtraList->HasType(RE::ExtraDataType::kEnableStateParent) : false;
		bool dyndolodFire = a_data.dyndolodFire;

		if (hasEnableParents || (hasEnableChildren && !dyndolodFire)) {
			return;
		}
		else if (hasEnableChildren && dyndolodFire) {
			std::string editorID = clib_util::editorID::get_editorID(a_fire->GetBaseObject()->As<RE::TESForm>());
			if (!editorID.contains("DYNDOLOD")) return;
		}
		else if (hasEnableChildren) {
			return;
		}

		auto offBoundForm = offForm->As<RE::TESBoundObject>();
		if (!offBoundForm) {
			this->ManipulateFireRegistry(a_fire, false);
			return;
		}
		if (!this->ManipulateFireRegistry(a_fire, true)) return;

		auto settingsSingleton = CachedData::FireRegistry::GetSingleton();

		if (settingsSingleton->GetCheckOcclusion()) {
			//TODO: Include this.
		}

		if (settingsSingleton->GetCheckLights()) {
			auto foundLight = GetNearestReferenceOfType(a_fire, settingsSingleton->GetLightSearchDistance(), RE::FormType::Light, false);
			if (foundLight) {
				additionalExtinguishes.push_back(foundLight);
			}
		}

		if (settingsSingleton->GetCheckSmoke()) {
			auto foundSmoke = GetNearestReferenceOfType(a_fire, settingsSingleton->GetSmokeSearchDistance(), RE::FormType::MovableStatic, true);
			if (foundSmoke) additionalExtinguishes.push_back(foundSmoke);
		}

		if (dyndolodFire) {
			std::string editorID = clib_util::editorID::get_editorID(offForm);
			auto* baseFire = a_fire->GetBaseObject()->As<RE::TESForm>();
			std::string baseEDID = clib_util::editorID::get_editorID(baseFire);

			if (!(baseEDID.empty() || baseEDID.contains("DYNDOLOD"))) {
				RE::TESObjectREFR* dyndolodREF = GetNearestMatchingDynDOLODFire(a_fire, 200.0f, baseEDID + "_DYNDOLOD_BASE");
				if (!dyndolodREF) {
					additionalExtinguishes.push_back(dyndolodREF);
				}
			}
		}

		auto offFire = a_fire->PlaceObjectAtMe(offBoundForm, false);
		auto* offReference = offFire.get();
		this->ManipulateFireRegistry(offReference, true);
		offReference->MoveTo(a_fire);
		offReference->data.angle = a_fire->data.angle;
		offReference->refScale = a_fire->refScale;
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* handlePolicy = vm->GetObjectHandlePolicy();
		RE::VMHandle handle = handlePolicy->GetHandleForObject(offReference->GetFormType(), offReference);
		if (!handle || !vm->attachedScripts.contains(handle)) {
			this->ManipulateFireRegistry(a_fire, false);
			offReference->Disable();
			offReference->DeleteThis();
			return;
		}

		bool success = false;

		for (auto& foundScript : vm->attachedScripts.find(handle)->second) {
			if (foundScript->GetTypeInfo()->GetName() != "REF_ObjectRefOffController"sv) continue;
			auto relatedFlame = foundScript->GetProperty("RelatedFlame");
			auto addExtProperty = foundScript->GetProperty("RelatedObjects");
			auto dayAttached = foundScript->GetProperty("DayAttached");
			if (!(relatedFlame && addExtProperty && dayAttached)) {
				continue;
			}

			RE::BSScript::PackValue(relatedFlame, a_fire);
			RE::BSScript::PackValue(addExtProperty, additionalExtinguishes);
			dayAttached->SetFloat(RE::Calendar::GetSingleton()->GetDaysPassed());
			auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
			auto args = RE::MakeFunctionArguments();
			const RE::BSFixedString functionName = "Extinguish"sv;
			auto scriptObject = foundScript.get();
			auto object = RE::BSTSmartPointer<RE::BSScript::Object>(scriptObject);
			vm->DispatchMethodCall(object, functionName, args, callback);
			success = true;
			break;
		}

		if (!success) {
			if (a_fire->IsDisabled()) {
				a_fire->Enable(false);
			}
			offReference->Disable();
			offReference->DeleteThis();
			this->ManipulateFireRegistry(a_fire, false);
			this->ManipulateFireRegistry(offReference, false);
		}
	}

	void Papyrus::ExtinguishAllFires() {
		if (const auto TES = RE::TES::GetSingleton(); TES) {
			TES->ForEachReferenceInRange(RE::PlayerCharacter::GetSingleton()->AsReference(), 0.0, [&](RE::TESObjectREFR* a_ref) {
				if (!a_ref->Is3DLoaded()) return RE::BSContainer::ForEachResult::kContinue;

				auto* referenceBoundObject = a_ref ? a_ref->GetBaseObject() : nullptr;
				auto* referenceBaseObject = referenceBoundObject ? referenceBoundObject->As<RE::TESForm>() : nullptr;
				if (!(referenceBaseObject && CachedData::FireRegistry::GetSingleton()->IsOnFire(referenceBaseObject))) {
					return RE::BSContainer::ForEachResult::kContinue;
				}

				CachedData::FireData fireData = CachedData::FireRegistry::GetSingleton()->GetOffForm(referenceBaseObject);
				if (fireData.offVersion) {
					Papyrus::Papyrus::GetSingleton()->ExtinguishFire(a_ref, fireData);
				}
				return RE::BSContainer::ForEachResult::kContinue;
				});
		}
	}

	void Papyrus::SetIsRaining(bool a_isRaining) {
		if (this->disable) return;
		this->isRaining = a_isRaining;
	}

	void Papyrus::Papyrus::DisablePapyrus() {
		this->disable = true;
	}
}