#ifdef DZ_Expansion_Navigation
modded class ExpansionMapMenu
{
	protected ref ZenMapFogRenderer m_ZenMapFogRenderer;
	protected Widget m_ZenMapFogOriginalToolsContainer;
	protected Widget m_ZenMapFogToolsOverlay;
	protected bool m_ZenMapFogToolsOverlayFailureLogged;

	override Widget Init()
	{
		Widget root = super.Init();

		if (root)
		{
			ZenMapFog_SetupExpansionToolsOverlay(root);

			if (!m_ZenMapFogRenderer && GetMap())
			{
				m_ZenMapFogRenderer = new ZenMapFogRenderer(GetMap(), root);
			}

			ZenMapFog_KeepExpansionUIAboveFog();
		}

		return root;
	}

	override void OnShow()
	{
		super.OnShow();

		if (m_ZenMapFogRenderer)
		{
			m_ZenMapFogRenderer.Show();
		}

		ZenMapFog_KeepExpansionUIAboveFog();
	}

	override void OnHide()
	{
		if (m_ZenMapFogRenderer)
		{
			m_ZenMapFogRenderer.Hide();
		}

		super.OnHide();
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		if (m_ZenMapFogRenderer)
		{
			m_ZenMapFogRenderer.Update(timeslice);
		}

		ZenMapFog_KeepExpansionUIAboveFog();
	}

	protected void ZenMapFog_SetupExpansionToolsOverlay(Widget root)
	{
		if (!root)
		{
			return;
		}

		// Expansion's SetMapLegend() runs inside super.Init() and may re-bind these
		// protected members to the original nested tools widget each time Init runs.
		// Snapshot its current visibility before redirecting the members to our clone.
		Widget originalUpperLegend = m_UpperLegendContainer;
		ImageWidget originalCompassArrow = m_ToolsCompassArrow;
		TextWidget originalCompassAzimuth = m_ToolsCompassAzimuth;

		m_ZenMapFogOriginalToolsContainer = root.FindAnyWidget("Map_Tools_Container");

		if (!m_ZenMapFogToolsOverlay)
		{
			m_ZenMapFogToolsOverlay = g_Game.GetWorkspace().CreateWidgets("ZenMapFog/GUI/layouts/zen_expansion_map_tools_overlay.layout", root);
			if (!m_ZenMapFogToolsOverlay)
			{
				if (!m_ZenMapFogToolsOverlayFailureLogged)
				{
					m_ZenMapFogToolsOverlayFailureLogged = true;
					Print("[ZenMapFog] ERROR: Failed to create Expansion tools overlay. Leaving original tools widget enabled.");
				}

				return;
			}

			Print("[ZenMapFog] Created root-level Expansion tools overlay above fog.");
		}

		m_ZenMapFogToolsOverlay.SetSort(900);

		SizeToChild overlayLegendResizer;
		m_ZenMapFogToolsOverlay.GetScript(overlayLegendResizer);

		Widget overlayUpperLegend = m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Extra");
		ImageWidget overlayCompassBase = ImageWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Compass_Base"));
		ImageWidget overlayCompassArrow = ImageWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Compass_Arrow"));
		TextWidget overlayCompassAzimuth = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Compass_Azimuth"));
		TextWidget overlayGPSX = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_GPS_X_Value"));
		TextWidget overlayGPSY = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_GPS_Y_Value"));
		TextWidget overlayGPSElevation = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_GPS_Elevation_Value"));
		TextWidget overlayScaleContour = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Scale_Contour_Value"));
		TextWidget overlayScaleCellSize = TextWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Scale_CellSize_Value"));
		CanvasWidget overlayScaleCanvas = CanvasWidget.Cast(m_ZenMapFogToolsOverlay.FindAnyWidget("ZMF_Tools_Scale_CellSize_Canvas"));

		if (!overlayLegendResizer || !overlayUpperLegend || !overlayCompassBase || !overlayCompassArrow || !overlayCompassAzimuth || !overlayGPSX || !overlayGPSY || !overlayGPSElevation || !overlayScaleContour || !overlayScaleCellSize || !overlayScaleCanvas)
		{
			if (!m_ZenMapFogToolsOverlayFailureLogged)
			{
				m_ZenMapFogToolsOverlayFailureLogged = true;
				Print("[ZenMapFog] ERROR: Expansion tools overlay is missing one or more required widgets. Leaving original tools widget enabled.");
			}

			m_ZenMapFogToolsOverlay.Show(false);
			return;
		}

		m_ZenMapFogToolsOverlayFailureLogged = false;

		// Match Expansion's initialization colors exactly.
		overlayCompassBase.SetColor(ARGB(255, 220, 220, 220));
		overlayCompassArrow.SetColor(ARGB(255, 220, 220, 220));

		// Preserve the visibility state Expansion calculated during SetMapLegend().
		if (originalUpperLegend)
		{
			overlayUpperLegend.Show(originalUpperLegend.IsVisible());
		}

		if (originalCompassArrow)
		{
			overlayCompassArrow.Show(originalCompassArrow.IsVisible());
		}

		if (originalCompassAzimuth)
		{
			overlayCompassAzimuth.Show(originalCompassAzimuth.IsVisible());
		}

		// Redirect Expansion's own update logic to our root-level duplicate. Expansion
		// continues calculating compass, grid sector, altitude, contour interval and
		// scale ruler exactly as normal; only the destination widgets change.
		m_LegendResizer = overlayLegendResizer;
		m_UpperLegendContainer = overlayUpperLegend;
		m_ToolsCompassBase = overlayCompassBase;
		m_ToolsCompassArrow = overlayCompassArrow;
		m_ToolsCompassAzimuth = overlayCompassAzimuth;
		m_ToolsGPSXText = overlayGPSX;
		m_ToolsGPSYText = overlayGPSY;
		m_ToolsGPSElevationText = overlayGPSElevation;
		m_ToolsScaleContourText = overlayScaleContour;
		m_ToolsScaleCellSizeText = overlayScaleCellSize;
		m_ToolsScaleCellSizeCanvas = overlayScaleCanvas;

		float canvasHeight = 0.0;
		m_ToolsScaleCellSizeCanvas.GetSize(m_ToolScaleCellSizeCanvasWidth, canvasHeight);

		m_ZenMapFogToolsOverlay.Show(true);
		m_LegendResizer.ResizeParentToChild();

		// Do not reparent or unlink Expansion's original native widget. Simply hide it.
		// This avoids the access violation caused by RemoveChild/AddChild while keeping
		// the underlying map fully covered by fog beneath the root-level duplicate.
		if (m_ZenMapFogOriginalToolsContainer && m_ZenMapFogToolsOverlay && !m_ZenMapFogToolsOverlayFailureLogged)
		{
			m_ZenMapFogOriginalToolsContainer.Show(false);
		}
	}

	protected void ZenMapFog_KeepExpansionUIAboveFog()
	{
		if (m_Markers)
		{
			foreach (ExpansionMapWidgetBase marker : m_Markers)
			{
				if (!marker)
				{
					continue;
				}

				Widget markerRoot = marker.GetLayoutRoot();
				if (markerRoot && markerRoot.GetSort() < 750)
				{
					markerRoot.SetSort(750);
				}
			}
		}

		if (m_ZenMapFogToolsOverlay && !m_ZenMapFogToolsOverlayFailureLogged)
		{
			if (m_ZenMapFogToolsOverlay.GetSort() < 900)
			{
				m_ZenMapFogToolsOverlay.SetSort(900);
			}

			m_ZenMapFogToolsOverlay.Show(true);
		}

		if (m_ZenMapFogOriginalToolsContainer && m_ZenMapFogToolsOverlay && !m_ZenMapFogToolsOverlayFailureLogged)
		{
			m_ZenMapFogOriginalToolsContainer.Show(false);
		}
	}
};
#endif
