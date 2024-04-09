#pragma once
#include "FormData.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace LinkedDistribution
{

	/// <summary>
	/// Scope of a linked form determines which distributions can trigger linked forms.
	/// </summary>
	enum Scope : std::uint8_t
	{
		/// <summary>
		/// Local scope links forms only to distributions defined in the same configuration file.
		/// </summary>
		kLocal = 0,

		/// <summary>
		/// Global scope links forms to all distributions in all loaded configuration files.
		/// </summary>
		kGlobal
	};

	/// <summary>
	/// Type of the distribution that is being linked to.
	/// </summary>
	enum DistributionType : std::uint8_t
	{
		/// <summary>
		/// Regular distribution that occurs during normal distribution pass.
		/// </summary>
		kRegular,

		/// <summary>
		/// Distribution that occurs when an NPC dies.
		/// </summary>
		kDeath
	};

	namespace INI
	{
		struct RawLinkedForm
		{
			FormOrEditorID formOrEditorID{};

			Scope scope{ kLocal };

			/// Raw filters in RawLinkedForm only use MATCH, there is no meaning for ALL or NOT, so they are ignored.
			Filters<FormOrEditorID> formIDs{};

			IndexOrCount  idxOrCount{ RandomCount(1, 1) };
			PercentChance chance{ 100 };

			Path path{};
		};

		using LinkedFormsVec = std::vector<RawLinkedForm>;
		using LinkedFormsConfig = std::unordered_map<DistributionType, std::unordered_map<RECORD::TYPE, LinkedFormsVec>>;

		inline LinkedFormsConfig linkedForms{};

		/// <summary>
		/// Checks whether given entry is a linked form and attempts to parse it.
		/// </summary>
		/// <returns>true if given entry was a linked form. Note that returned value doesn't represent whether parsing was successful.</returns>
		bool TryParse(const std::string& key, const std::string& value, const Path&);
	}

	using namespace Forms;

	class Manager;

	template <class Form>
	struct LinkedForms;

	namespace detail
	{
		template <class Form = RE::TESForm>
		Form* LookupLinkedForm(RE::TESDataHandler* const dataHandler, INI::RawLinkedForm& rawForm);
	}

	template <class Form>
	struct LinkedForms
	{
		friend Manager;  // allow Manager to later modify forms directly.
		friend Form* detail::LookupLinkedForm(RE::TESDataHandler* const, INI::RawLinkedForm&);

		using FormsMap = std::unordered_map<DistributionType, std::unordered_map<Path, std::unordered_map<RE::TESForm*, DataVec<Form>>>>;

		LinkedForms(RECORD::TYPE type) :
			type(type)
		{}

		RECORD::TYPE    GetType() const { return type; }
		const FormsMap& GetForms() const { return forms; }

		void LookupForms(RE::TESDataHandler* const, DistributionType, INI::LinkedFormsVec& rawLinkedForms);

	private:
		RECORD::TYPE type;
		FormsMap     forms{};

		void Link(Form*, Scope, DistributionType, const FormVec& linkedForms, const IndexOrCount&, const PercentChance&, const Path&);
	};

	class Manager : public ISingleton<Manager>
	{
	public:
		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid linked forms.
		/// </summary>
		/// <param name="dataHandler">A DataHandler that will perform the actual lookup.</param>
		/// <param name="rawLinkedDistribution">A raw linked form entries that should be processed.</param>
		void LookupLinkedForms(RE::TESDataHandler* const, INI::LinkedFormsConfig& rawLinkedForms = INI::linkedForms);

		void LogLinkedFormsLookup();

		/// <summary>
		/// Calculates DistributionSet for each linked form and calls a callback for each of them.
		/// </summary>
		/// <param name="distributionType">Type of the distribution for which linked sets should be returned.</param>
		/// <param name="linkedForms">A set of forms for which distribution sets should be calculated.
		///							  This is typically distributed forms accumulated during first distribution pass.</param>
		/// <param name="distribute">A callback to be called with each DistributionSet. This is supposed to do the actual distribution.</param>
		void ForEachLinkedDistributionSet(DistributionType, const DistributedForms& linkedForms, std::function<void(DistributionSet&)> distribute);

		bool IsEmpty(DistributionType) const;

	private:
		template <class Form>
		DataVec<Form>& LinkedFormsForForm(DistributionType, const DistributedForm&, Scope, LinkedForms<Form>&) const;

		void LookupLinkedForms(RE::TESDataHandler* const, DistributionType, INI::LinkedFormsConfig& rawLinkedForms);
		void LogLinkedFormsLookup(DistributionType);

		void ForEachLinkedDistributionSet(DistributionType, const DistributedForms& linkedForms, Scope, std::function<void(DistributionSet&)> distribute);

		LinkedForms<RE::SpellItem>      spells{ RECORD::kSpell };
		LinkedForms<RE::BGSPerk>        perks{ RECORD::kPerk };
		LinkedForms<RE::TESBoundObject> items{ RECORD::kItem };
		LinkedForms<RE::TESShout>       shouts{ RECORD::kShout };
		LinkedForms<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
		LinkedForms<RE::TESForm>        packages{ RECORD::kPackage };
		LinkedForms<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
		LinkedForms<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
		LinkedForms<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
		LinkedForms<RE::TESFaction>     factions{ RECORD::kFaction };
		LinkedForms<RE::TESObjectARMO>  skins{ RECORD::kSkin };

		/// <summary>
		/// Iterates over each type of LinkedForms and calls a callback with each of them.
		/// </summary>
		template <typename Func, typename... Args>
		void ForEachLinkedForms(Func&& func, Args&&... args);
	};

#pragma region Implementation

	template <class Form>
	Form* detail::LookupLinkedForm(RE::TESDataHandler* const dataHandler, INI::RawLinkedForm& rawForm)
	{
		using namespace Forms::Lookup;

		try {
			return Forms::detail::get_form<Form>(dataHandler, rawForm.formOrEditorID, rawForm.path, LookupOptions::kCreateIfMissing);
		} catch (const UnknownFormIDException& e) {
			buffered_logger::error("\t\t[{}] LinkedForm [0x{:X}] ({}) SKIP - formID doesn't exist", e.path, e.formID, e.modName.value_or(""));
		} catch (const UnknownPluginException& e) {
			buffered_logger::error("\t\t[{}] LinkedForm ({}) SKIP - mod cannot be found", e.path, e.modName);
		} catch (const InvalidKeywordException& e) {
			buffered_logger::error("\t\t[{}] LinkedForm [0x{:X}] ({}) SKIP - keyword does not have a valid editorID", e.path, e.formID, e.modName.value_or(""));
		} catch (const KeywordNotFoundException& e) {
			if (e.isDynamic) {
				buffered_logger::critical("\t\t[{}] LinkedForm {} FAIL - couldn't create keyword", e.path, e.editorID);
			} else {
				buffered_logger::critical("\t\t[{}] LinkedForm {} FAIL - couldn't get existing keyword", e.path, e.editorID);
			}
		} catch (const UnknownEditorIDException& e) {
			buffered_logger::error("\t\t[{}] LinkedForm ({}) SKIP - editorID doesn't exist", e.path, e.editorID);
		} catch (const MalformedEditorIDException& e) {
			buffered_logger::error("\t\t[{}] LinkedForm (\"\") SKIP - malformed editorID", e.path);
		} catch (const MismatchingFormTypeException& e) {
			std::visit(overload{
						   [&](const FormModPair& formMod) {
							   auto& [formID, modName] = formMod;
							   buffered_logger::error("\t\t[{}] LinkedForm [0x{:X}] ({}) SKIP - mismatching form type (expected: {}, actual: {})", e.path, *formID, modName.value_or(""), e.expectedFormType, e.actualFormType);
						   },
						   [&](std::string editorID) {
							   buffered_logger::error("\t\t[{}] LinkedForm ({}) SKIP - mismatching form type (expected: {}, actual: {})", e.path, editorID, e.expectedFormType, e.actualFormType);
						   } },
				e.formOrEditorID);
		} catch (const InvalidFormTypeException& e) {
			std::visit(overload{
						   [&](const FormModPair& formMod) {
							   auto& [formID, modName] = formMod;
							   buffered_logger::error("\t\t[{}] LinkedForm [0x{:X}] ({}) SKIP - unsupported form type ({})", e.path, *formID, modName.value_or(""), e.formType);
						   },
						   [&](std::string editorID) {
							   buffered_logger::error("\t\t[{}] LinkedForm ({}) SKIP - unsupported form type ({})", e.path, editorID, e.formType);
						   } },
				e.formOrEditorID);
		}
		return nullptr;
	}

	template <class Form>
	DataVec<Form>& Manager::LinkedFormsForForm(DistributionType type, const DistributedForm& form, Scope scope, LinkedForms<Form>& linkedForms) const
	{
		auto& forms = linkedForms.forms[type];
		if (const auto formsIt = forms.find(scope == kLocal ? form.second : ""); formsIt != forms.end()) {
			if (const auto linkedFormsIt = formsIt->second.find(form.first); linkedFormsIt != formsIt->second.end()) {
				return linkedFormsIt->second;
			}
		}

		static DataVec<Form> empty{};
		return empty;
	}

	template <typename Func, typename... Args>
	void Manager::ForEachLinkedForms(Func&& func, Args&&... args)
	{
		func(keywords, std::forward<Args>(args)...);
		func(spells, std::forward<Args>(args)...);
		func(levSpells, std::forward<Args>(args)...);
		func(perks, std::forward<Args>(args)...);
		func(shouts, std::forward<Args>(args)...);
		func(items, std::forward<Args>(args)...);
		func(outfits, std::forward<Args>(args)...);
		func(sleepOutfits, std::forward<Args>(args)...);
		func(factions, std::forward<Args>(args)...);
		func(packages, std::forward<Args>(args)...);
		func(skins, std::forward<Args>(args)...);
	}

	template <class Form>
	void LinkedForms<Form>::LookupForms(RE::TESDataHandler* const dataHandler, DistributionType type, INI::LinkedFormsVec& rawLinkedForms)
	{
		for (auto& rawForm : rawLinkedForms) {
			if (auto form = detail::LookupLinkedForm<Form>(dataHandler, rawForm); form) {
				auto& [formID, scope, parentFormIDs, count, chance, path] = rawForm;
				FormVec parentForms{};
				if (Forms::detail::formID_to_form(dataHandler, parentFormIDs.MATCH, parentForms, path, LookupOptions::kNone)) {
					Link(form, scope, type, parentForms, count, chance, path);
				}
			}
		}
	}

	template <class Form>
	void LinkedForms<Form>::Link(Form* form, Scope scope, DistributionType type, const FormVec& linkedForms, const IndexOrCount& idxOrCount, const PercentChance& chance, const Path& path)
	{
		for (const auto& linkedForm : linkedForms) {
			if (std::holds_alternative<RE::TESForm*>(linkedForm)) {
				auto& distributableFormsAtPath = forms[type][scope == kLocal ? path : ""];  // If item is global, we put it in a common map with no information about the path.
				auto& distributableForms = distributableFormsAtPath[std::get<RE::TESForm*>(linkedForm)];
				// Note that we don't use Data.index here, as these linked forms don't have any leveled filters
				// and as such do not to track their index.
				distributableForms.emplace_back(0, form, idxOrCount, FilterData({}, {}, {}, {}, chance), path);
			}
		}
	}
#pragma endregion
}
