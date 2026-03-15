#pragma once

namespace Distribute::PlayerLeveledActor
{
	namespace detail
	{
		template <class Form>
		auto set_to_vec(const Set<RE::FormID>& a_formIDSet) -> std::vector<Form*>
		{
			std::vector<Form*> forms{};
			forms.reserve(a_formIDSet.size());
			for (auto& formID : a_formIDSet) {
				if (auto* form = RE::TESForm::LookupByID<Form>(formID)) {
					forms.emplace_back(form);
				}
			}
			return forms;
		}
	}

	void Install();
}
