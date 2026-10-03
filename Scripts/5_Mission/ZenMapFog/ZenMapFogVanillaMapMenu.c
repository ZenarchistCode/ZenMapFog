modded class MapMenu
{
	protected ref ZenMapFogRenderer m_ZenMapFogVanillaRenderer;
	protected Widget m_ZenMapFogVanillaToolsBackground;

	override void OnShow()
	{
		super.OnShow();

		if (!ZenMapFog_IsVanillaMapMenuActive())
			return;

		if (!m_ZenMapFogVanillaToolsBackground && m_MapToolsContainer)
			m_ZenMapFogVanillaToolsBackground = g_Game.GetWorkspace().CreateWidgets("ZenMapFog/GUI/layouts/zen_vanilla_map_tools_background.layout", m_MapToolsContainer);

		if (!m_ZenMapFogVanillaRenderer && m_MapWidgetInstance && GetLayoutRoot())
		{
			// Vanilla's map and tools share a parent. Keep fog in that same layer
			// so the tools' higher sort order puts their background and text above it.
			Widget fogParent = m_MapWidgetInstance.GetParent();
			if (!fogParent)
				fogParent = GetLayoutRoot();

			m_ZenMapFogVanillaRenderer = new ZenMapFogRenderer(m_MapWidgetInstance, fogParent);
		}

		if (m_ZenMapFogVanillaRenderer)
			m_ZenMapFogVanillaRenderer.Show();

		ZenMapFog_KeepVanillaMapUIAboveFog();
	}

	override void OnHide()
	{
		if (m_ZenMapFogVanillaRenderer)
			m_ZenMapFogVanillaRenderer.Hide();

		super.OnHide();
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		if (!ZenMapFog_IsVanillaMapMenuActive())
			return;

		if (m_ZenMapFogVanillaRenderer)
			m_ZenMapFogVanillaRenderer.Update(timeslice);

		ZenMapFog_KeepVanillaMapUIAboveFog();
	}

	protected bool ZenMapFog_IsVanillaMapMenuActive()
	{
		UIScriptedMenu vanillaMapMenu = g_Game.GetUIManager().FindMenu(MENU_MAP);
		return vanillaMapMenu && vanillaMapMenu == this;
	}

	protected void ZenMapFog_KeepVanillaMapUIAboveFog()
	{
		if (m_MapToolsContainer && m_MapToolsContainer.GetSort() < 900)
			m_MapToolsContainer.SetSort(900);

		if (m_ToolbarPanel && m_ToolbarPanel.GetSort() < 900)
			m_ToolbarPanel.SetSort(900);
	}
};
