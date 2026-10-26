class ActionReadZenMapFogRevealerCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(10.0);
	}
};

class ActionReadZenMapFogRevealer : ActionContinuousBase
{
	void ActionReadZenMapFogRevealer()
	{
		m_CallbackClass = ActionReadZenMapFogRevealerCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_VIEWNOTE;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_PRONE;
		m_Text = "#read";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined;
		m_ConditionTarget = new CCTNone;
	}

	override bool HasTarget()
	{
		return false;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!player || !player.IsAlive() || player.IsUnconscious())
			return false;

		ZenMapFogRevealer revealer = ZenMapFogRevealer.Cast(item);
		return revealer && !revealer.IsRuined() && player.GetItemInHands() == revealer;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (!g_Game.IsServer() || !action_data || !ActionCondition(action_data.m_Player, action_data.m_Target, action_data.m_MainItem))
			return;

		// Use the existing saved reveal and client sync; consume only on success.
		if (ZenMapFogServerManager.Get().RevealEntireMap(action_data.m_Player))
			action_data.m_MainItem.SetHealth("", "Health", 0);
	}
};
