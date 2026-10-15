#pragma once

namespace hksr::sdk_v1::detail::catalog_bindings {
// Qualified 4.6 declaration identities. Readers keep semantic field discovery,
// ownership and traversal in src; an update to these names is an adapter change.
inline constexpr auto visual_table="LCOMIBOIAPN";
inline constexpr auto prepare_visual_row="PEJCNLBKIGN";
inline constexpr auto animation_names="NNMPPHHHEIK";
inline constexpr auto reaction_name="ECACKGEBFGC";
inline constexpr auto default_reaction_name="PJKNFGDJCAK";
inline constexpr auto ready_transition="AODEJIKGNFJ";
inline constexpr auto default_expression="CKPEADONMLA";
inline constexpr auto empty_layer="MIOIOHJKFEE";
inline constexpr auto hit_additive_layer="PIOGPCGJJAP";
inline constexpr auto hit_override_layer="AIGNNOJHKPJ";
inline constexpr auto npc_motions="AJKNMKCMKHB";
inline constexpr auto motion_names="KCPFICBNOMD";
inline constexpr auto motion_id="MCKEFAANJNI";
inline constexpr auto motion_row="LMBKNCAIDLC";
// 4.6 CA154CA -> CA10A50..A6B: normalized expression blend 0.2.
// Stun behavior selects the independently observed blink source 3.
inline constexpr float expression_transition=0.2f;
inline constexpr auto blink_expression="Emo_Stun";
inline constexpr int blink_source=3;
}
