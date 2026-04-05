#include "Hooks.h"

#include "Manager.h"

namespace Hooks {
    namespace detail {
        using GetActivateTextFunc = bool (*)(RE::TESBoundObject*, RE::TESObjectREFR*, RE::BSString&);
        using ActivateFunc = bool (*)(RE::TESBoundObject*, RE::TESObjectREFR*, RE::TESObjectREFR*, std::uint8_t,
                                      RE::TESBoundObject*, std::int32_t);

        static GetActivateTextFunc resolved_gat_ARMO = nullptr;
        static ActivateFunc resolved_act_ARMO = nullptr;
        static GetActivateTextFunc resolved_gat_WEAP = nullptr;
        static ActivateFunc resolved_act_WEAP = nullptr;
        static GetActivateTextFunc resolved_gat_INGR = nullptr;
        static ActivateFunc resolved_act_INGR = nullptr;
        static GetActivateTextFunc resolved_gat_ALCH = nullptr;
        static ActivateFunc resolved_act_ALCH = nullptr;
        static GetActivateTextFunc resolved_gat_SCRL = nullptr;
        static ActivateFunc resolved_act_SCRL = nullptr;
        static GetActivateTextFunc resolved_gat_LIGH = nullptr;
        static ActivateFunc resolved_act_LIGH = nullptr;
        static GetActivateTextFunc resolved_gat_AMMO = nullptr;
        static ActivateFunc resolved_act_AMMO = nullptr;
        static GetActivateTextFunc resolved_gat_MISC = nullptr;
        static ActivateFunc resolved_act_MISC = nullptr;
        static GetActivateTextFunc resolved_gat_SLGM = nullptr;
        static ActivateFunc resolved_act_SLGM = nullptr;
        static GetActivateTextFunc resolved_gat_KEYM = nullptr;
        static ActivateFunc resolved_act_KEYM = nullptr;
        static GetActivateTextFunc resolved_gat_BOOK = nullptr;
        static ActivateFunc resolved_act_BOOK = nullptr;

        static std::string get_gold_label() {
            const auto* setting = RE::GameSettingCollection::GetSingleton()->GetSetting("sGold");
            return setting ? setting->GetString() : "gold";
        }

        static std::string make_buy_label(RE::TESObjectREFR* a_ref, const std::string& a_buyLabel) {
            const auto count = a_ref->extraList.GetCount();
            const auto cost = Manager::GetSingleton()->GetBuyCost(a_ref);
            const auto name = a_ref->GetDisplayFullName();
            const std::string goldLabel = get_gold_label();

            const std::string header = cost > 0 ? (Manager::GetSingleton()->GetGoldValueBelowBuyVisuals()
                                                       ? std::format("{}\n({} {})", a_buyLabel, cost, goldLabel)
                                                       : std::format("{} ({} {})", a_buyLabel, cost, goldLabel))
                                                : a_buyLabel;

            return count > 1 ? std::format("{}\n{} ({})", header, name, count) : std::format("{}\n{}", header, name);
        }

        static std::string make_take_label(RE::TESObjectREFR* a_ref) {
            const bool isSteal = a_ref->IsCrimeToActivate();
            const auto* setting = isSteal ? RE::GameSettingCollection::GetSingleton()->GetSetting("sSteal")
                                          : RE::GameSettingCollection::GetSingleton()->GetSetting("sTake");
            const std::string verb = setting ? setting->GetString() : (isSteal ? "Steal" : "Take");
            const auto count = a_ref->extraList.GetCount();
            const auto name = a_ref->GetDisplayFullName();

            return count > 1 ? std::format("{}\n{} ({})", verb, name, count) : std::format("{}\n{}", verb, name);
        }

        static void set_player_owner(RE::TESObjectREFR* a_ref, RE::Actor* a_player) {
            auto* playerBase = a_player->GetActorBase();
            if (!playerBase) {
                return;
            }

            if (auto* xOwner = a_ref->extraList.GetByType<RE::ExtraOwnership>()) {
                xOwner->owner = playerBase;
            } else {
                auto* newOwner = new RE::ExtraOwnership(playerBase);
                a_ref->extraList.Add(newOwner);
            }
        }

        static void mark_for_respawn(RE::TESObjectREFR* a_ref) {
            if (!a_ref) {
                return;
            }
            a_ref->formFlags |= static_cast<std::uint32_t>(RE::TESObjectREFR::RecordFlags::kRespawns);
        }

        static void give_gold_to_owner(RE::TESObjectREFR* a_ref, std::uint32_t a_cost) {
            if (a_cost == 0 || !a_ref) {
                return;
            }

            const auto goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
            if (!goldForm) {
                return;
            }

            RE::Actor* merchantActor = nullptr;

            RE::TESForm* owner = a_ref->extraList.GetOwner();
            if (owner) {
                const auto* cell = a_ref->GetParentCell();
                if (!cell) {
                    return;
                }

                if (const auto* ownerNPC = owner->As<RE::TESNPC>()) {
                    for (const auto& refHandle : cell->GetRuntimeData().references) {
                        const auto ref = refHandle.get();
                        if (!ref) {
                            continue;
                        }
                        auto* actor = ref->As<RE::Actor>();
                        if (!actor || actor->IsDead()) {
                            continue;
                        }
                        if (actor->GetActorBase() == ownerNPC) {
                            merchantActor = actor;
                            break;
                        }
                    }
                } else if (const auto* ownerFaction = owner->As<RE::TESFaction>()) {
                    for (const auto& refHandle : cell->GetRuntimeData().references) {
                        const auto ref = refHandle.get();
                        if (!ref) {
                            continue;
                        }
                        auto* actor = ref->As<RE::Actor>();
                        if (!actor || actor->IsDead()) {
                            continue;
                        }
                        const auto* npc = actor->GetActorBase();
                        if (!npc) {
                            continue;
                        }
                        for (const auto& factionRank : npc->factions) {
                            if (factionRank.faction == ownerFaction) {
                                merchantActor = actor;
                                break;
                            }
                        }
                        if (merchantActor) {
                            break;
                        }
                    }
                }
            } else {
                merchantActor = Manager::GetSingleton()->FindCellMerchant(a_ref);
            }

            if (merchantActor) {
                merchantActor->AddObjectToContainer(goldForm, nullptr, static_cast<std::int32_t>(a_cost), nullptr);
                logger::debug("Gave {} gold to merchant '{}'", a_cost, merchantActor->GetDisplayFullName());
            }
        }

        static bool attempt_purchase(RE::Actor* a_actor, RE::TESObjectREFR* a_ref, std::int32_t a_count) {
            const auto cost = Manager::GetSingleton()->GetBuyCost(a_ref);
            const auto goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
            if (!goldForm) {
                return false;
            }

            const auto playerInv = a_actor->GetInventoryCounts();
            const auto goldIt = playerInv.find(goldForm);
            const auto playerGold = (goldIt != playerInv.end()) ? static_cast<std::uint32_t>(goldIt->second) : 0u;

            if (playerGold < cost) {
                RE::DebugMessageBox(Manager::GetSingleton()->GetInsufficientGoldMessage().c_str());
                return false;
            }

            if (cost > 0) {
                a_actor->RemoveItem(goldForm, static_cast<std::int32_t>(cost), RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                                    nullptr);

                RE::BSAudioManager::GetSingleton()->Play(0x0003E952);

                give_gold_to_owner(a_ref, cost);
            }

            mark_for_respawn(a_ref);
            set_player_owner(a_ref, a_actor);
            a_actor->PickUpObject(a_ref, a_count, false, true);

            return true;
        }

        template <typename TObj>
        static void capture_vtable(GetActivateTextFunc& a_gat, ActivateFunc& a_act, std::size_t a_gatIdx,
                                   std::size_t a_actIdx) {
            const auto vtblAddr = TObj::VTABLE[0].address();
            if (vtblAddr) {
                const auto* vtable = reinterpret_cast<uintptr_t*>(vtblAddr);
                a_gat = reinterpret_cast<GetActivateTextFunc>(vtable[a_gatIdx]);
                a_act = reinterpret_cast<ActivateFunc>(vtable[a_actIdx]);
            }
        }
    }

    template <detail::GetActivateTextFunc* ResolvedGAT>
    struct GetActivateTextT {
        static bool thunk(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_activator, RE::BSString& a_dst) {
            const auto settings = Manager::GetSingleton();

            if (a_activator && settings->IsVendorItem(a_activator)) {
                if (settings->GetHotkeyPressed()) {
                    return (*ResolvedGAT)(a_this, a_activator, a_dst);
                } else {
                    a_dst = detail::make_buy_label(a_activator, settings->GetBuyLabel());
                }
                return true;
            }

            return (*ResolvedGAT)(a_this, a_activator, a_dst);
        }
        static inline REL::Relocation<decltype(thunk)> func;
        static inline constexpr std::size_t idx{0x4C};
    };

    template <detail::ActivateFunc* ResolvedAct>
    struct ActivateT {
        static bool thunk(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef,
                          std::uint8_t a_arg3, RE::TESBoundObject* /*a_objectToGet*/, std::int32_t a_targetCount) {
            if (const auto light = a_this->As<RE::TESObjectLIGH>(); light && !light->CanBeCarried()) {
                return false;
            }

            if (!a_targetRef || !a_activatorRef) {
                return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, nullptr, a_targetCount);
            }

            const auto actor = a_activatorRef->As<RE::Actor>();
            if (!actor || !actor->IsPlayerRef()) {
                return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, nullptr, a_targetCount);
            }

            const auto settings = Manager::GetSingleton();

            if (settings->IsVendorItem(a_targetRef)) {
                if (settings->GetHotkeyPressed()) {
                    return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, nullptr, a_targetCount);
                } else {
                    detail::attempt_purchase(actor, a_targetRef, a_targetCount);
                }
                return true;
            }

            return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, nullptr, a_targetCount);
        }
        static inline REL::Relocation<decltype(thunk)> func;
        static inline constexpr std::size_t idx{0x37};
    };

    // Book
    template <detail::GetActivateTextFunc* ResolvedGAT>
    struct GetActivateTextBookT {
        static bool thunk(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_activator, RE::BSString& a_dst) {
            const auto settings = Manager::GetSingleton();

            if (a_activator && settings->IsVendorItem(a_activator)) {
                if (settings->GetHotkeyPressed()) {
                    return (*ResolvedGAT)(a_this, a_activator, a_dst);
                } else {
                    a_dst = detail::make_buy_label(a_activator, settings->GetBuyLabel());
                }
                return true;
            }

            return (*ResolvedGAT)(a_this, a_activator, a_dst);
        }
        static inline REL::Relocation<decltype(thunk)> func;
        static inline constexpr std::size_t idx{0x4C};
    };

    template <detail::ActivateFunc* ResolvedAct>
    struct ActivateBookT {
        static bool thunk(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef,
                          std::uint8_t a_arg3, RE::TESBoundObject* a_objectToGet, std::int32_t a_targetCount) {
            if (!a_targetRef || !a_activatorRef) {
                return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, a_objectToGet, a_targetCount);
            }

            const auto actor = a_activatorRef->As<RE::Actor>();
            if (!actor || !actor->IsPlayerRef()) {
                return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, a_objectToGet, a_targetCount);
            }

            const auto settings = Manager::GetSingleton();

            if (settings->IsVendorItem(a_targetRef)) {
                if (settings->GetHotkeyPressed()) {
                    return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, a_objectToGet, a_targetCount);
                } else {
                    detail::attempt_purchase(actor, a_targetRef, a_targetCount);
                }
                return true;
            }

            return (*ResolvedAct)(a_this, a_targetRef, a_activatorRef, a_arg3, a_objectToGet, a_targetCount);
        }
        static inline REL::Relocation<decltype(thunk)> func;
        static inline constexpr std::size_t idx{0x37};
    };

    using GetActivateText_ARMO = GetActivateTextT<&detail::resolved_gat_ARMO>;
    using Activate_ARMO = ActivateT<&detail::resolved_act_ARMO>;
    using GetActivateText_WEAP = GetActivateTextT<&detail::resolved_gat_WEAP>;
    using Activate_WEAP = ActivateT<&detail::resolved_act_WEAP>;
    using GetActivateText_INGR = GetActivateTextT<&detail::resolved_gat_INGR>;
    using Activate_INGR = ActivateT<&detail::resolved_act_INGR>;
    using GetActivateText_ALCH = GetActivateTextT<&detail::resolved_gat_ALCH>;
    using Activate_ALCH = ActivateT<&detail::resolved_act_ALCH>;
    using GetActivateText_SCRL = GetActivateTextT<&detail::resolved_gat_SCRL>;
    using Activate_SCRL = ActivateT<&detail::resolved_act_SCRL>;
    using GetActivateText_LIGH = GetActivateTextT<&detail::resolved_gat_LIGH>;
    using Activate_LIGH = ActivateT<&detail::resolved_act_LIGH>;
    using GetActivateText_AMMO = GetActivateTextT<&detail::resolved_gat_AMMO>;
    using Activate_AMMO = ActivateT<&detail::resolved_act_AMMO>;
    using GetActivateText_MISC = GetActivateTextT<&detail::resolved_gat_MISC>;
    using Activate_MISC = ActivateT<&detail::resolved_act_MISC>;
    using GetActivateText_SLGM = GetActivateTextT<&detail::resolved_gat_SLGM>;
    using Activate_SLGM = ActivateT<&detail::resolved_act_SLGM>;
    using GetActivateText_KEYM = GetActivateTextT<&detail::resolved_gat_KEYM>;
    using Activate_KEYM = ActivateT<&detail::resolved_act_KEYM>;
    using GetActivateText_BOOK = GetActivateTextBookT<&detail::resolved_gat_BOOK>;
    using Activate_BOOK = ActivateBookT<&detail::resolved_act_BOOK>;

    void Install() {
        logger::info("{:*^30}", "HOOKS");

        // do for each hooks
        detail::capture_vtable<RE::TESObjectARMO>(detail::resolved_gat_ARMO, detail::resolved_act_ARMO,
                                                  GetActivateText_ARMO::idx, Activate_ARMO::idx);
        detail::capture_vtable<RE::TESObjectWEAP>(detail::resolved_gat_WEAP, detail::resolved_act_WEAP,
                                                  GetActivateText_WEAP::idx, Activate_WEAP::idx);
        detail::capture_vtable<RE::IngredientItem>(detail::resolved_gat_INGR, detail::resolved_act_INGR,
                                                   GetActivateText_INGR::idx, Activate_INGR::idx);
        detail::capture_vtable<RE::AlchemyItem>(detail::resolved_gat_ALCH, detail::resolved_act_ALCH,
                                                GetActivateText_ALCH::idx, Activate_ALCH::idx);
        detail::capture_vtable<RE::ScrollItem>(detail::resolved_gat_SCRL, detail::resolved_act_SCRL,
                                               GetActivateText_SCRL::idx, Activate_SCRL::idx);
        detail::capture_vtable<RE::TESObjectLIGH>(detail::resolved_gat_LIGH, detail::resolved_act_LIGH,
                                                  GetActivateText_LIGH::idx, Activate_LIGH::idx);
        detail::capture_vtable<RE::TESAmmo>(detail::resolved_gat_AMMO, detail::resolved_act_AMMO,
                                            GetActivateText_AMMO::idx, Activate_AMMO::idx);
        detail::capture_vtable<RE::TESObjectMISC>(detail::resolved_gat_MISC, detail::resolved_act_MISC,
                                                  GetActivateText_MISC::idx, Activate_MISC::idx);
        detail::capture_vtable<RE::TESSoulGem>(detail::resolved_gat_SLGM, detail::resolved_act_SLGM,
                                               GetActivateText_SLGM::idx, Activate_SLGM::idx);
        detail::capture_vtable<RE::TESKey>(detail::resolved_gat_KEYM, detail::resolved_act_KEYM,
                                           GetActivateText_KEYM::idx, Activate_KEYM::idx);
        detail::capture_vtable<RE::TESObjectBOOK>(detail::resolved_gat_BOOK, detail::resolved_act_BOOK,
                                                  GetActivateText_BOOK::idx, Activate_BOOK::idx);

        stl::write_vfunc<RE::TESObjectARMO, Activate_ARMO>();
        stl::write_vfunc<RE::TESObjectARMO, GetActivateText_ARMO>();
        logger::info("Registered armor hooks");

        stl::write_vfunc<RE::TESObjectWEAP, Activate_WEAP>();
        stl::write_vfunc<RE::TESObjectWEAP, GetActivateText_WEAP>();
        logger::info("Registered weapon hooks");

        stl::write_vfunc<RE::IngredientItem, Activate_INGR>();
        stl::write_vfunc<RE::IngredientItem, GetActivateText_INGR>();
        logger::info("Registered ingredient hooks");

        stl::write_vfunc<RE::AlchemyItem, Activate_ALCH>();
        stl::write_vfunc<RE::AlchemyItem, GetActivateText_ALCH>();
        logger::info("Registered alchemy hooks");

        stl::write_vfunc<RE::ScrollItem, Activate_SCRL>();
        stl::write_vfunc<RE::ScrollItem, GetActivateText_SCRL>();
        logger::info("Registered scroll hooks");

        stl::write_vfunc<RE::TESObjectLIGH, Activate_LIGH>();
        stl::write_vfunc<RE::TESObjectLIGH, GetActivateText_LIGH>();
        logger::info("Registered torch/light hooks");

        stl::write_vfunc<RE::TESAmmo, Activate_AMMO>();
        stl::write_vfunc<RE::TESAmmo, GetActivateText_AMMO>();
        logger::info("Registered ammo hooks");

        stl::write_vfunc<RE::TESObjectMISC, Activate_MISC>();
        stl::write_vfunc<RE::TESObjectMISC, GetActivateText_MISC>();
        logger::info("Registered misc object hooks");

        stl::write_vfunc<RE::TESObjectBOOK, Activate_BOOK>();
        stl::write_vfunc<RE::TESObjectBOOK, GetActivateText_BOOK>();
        logger::info("Registered book hooks");

        stl::write_vfunc<RE::TESSoulGem, Activate_SLGM>();
        stl::write_vfunc<RE::TESSoulGem, GetActivateText_SLGM>();
        logger::info("Registered soul gem hooks");

        stl::write_vfunc<RE::TESKey, Activate_KEYM>();
        stl::write_vfunc<RE::TESKey, GetActivateText_KEYM>();
        logger::info("Registered key hooks");
    }
}
