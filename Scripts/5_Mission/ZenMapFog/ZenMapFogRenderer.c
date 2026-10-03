class ZenMapFogRenderer
{
	protected static const int FOG_TYPE_NONE = 0;
	protected static const int FOG_TYPE_ADJACENT = 1;
	protected static const int FOG_TYPE_FULL = 2;
	protected static const int FOG_TYPE_ROUNDED_CUTOUT = 3;
	protected static const int FOG_TYPE_BOUNDARY_ROUNDED_CUTOUT = 4;
	protected static const int ROUNDED_CORNER_TOP_LEFT = 0;
	protected static const int ROUNDED_CORNER_TOP_RIGHT = 1;
	protected static const int ROUNDED_CORNER_BOTTOM_LEFT = 2;
	protected static const int ROUNDED_CORNER_BOTTOM_RIGHT = 3;
	protected static const int ROUNDED_EDGE_STEPS = 12;

	protected MapWidget m_MapWidget;
	protected Widget m_MenuRoot;
	protected ref array<Widget> m_FogWidgets;
	protected ref array<Widget> m_RoundedWidgets;
	protected ref array<Widget> m_GridWidgets;
	protected float m_LastScale = -1.0;
	protected vector m_LastMapPosition;
	protected int m_LastRevision = -1;
	protected float m_RenderAccumulator;
	protected bool m_Shown;
	protected bool m_CreateFailureLogged;
	protected bool m_OutsideTransformValid;
	protected int m_OutsideReferenceCellX;
	protected int m_OutsideReferenceCellZ;
	protected vector m_OutsideReferenceScreen;
	protected vector m_OutsideStepX;
	protected vector m_OutsideStepZ;

	void ZenMapFogRenderer(MapWidget mapWidget, Widget menuRoot)
	{
		m_MapWidget = mapWidget;
		m_MenuRoot = menuRoot;
		m_FogWidgets = new array<Widget>;
		m_RoundedWidgets = new array<Widget>;
		m_GridWidgets = new array<Widget>;
	}

	void ~ZenMapFogRenderer()
	{
		UnlinkWidgets(m_FogWidgets);
		UnlinkWidgets(m_RoundedWidgets);
		UnlinkWidgets(m_GridWidgets);
	}

	void Show()
	{
		m_Shown = true;
		m_LastScale = -1.0;
		m_LastRevision = -1;
		m_RenderAccumulator = ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS;
		ZenMapFogClientData.Get().RequestSyncIfNeeded();
	}

	void Hide()
	{
		m_Shown = false;
		HideWidgets(m_FogWidgets, 0);
		HideWidgets(m_RoundedWidgets, 0);
		HideWidgets(m_GridWidgets, 0);
	}

	void Update(float timeslice)
	{
		if (!m_Shown || !m_MapWidget || !m_MenuRoot)
			return;

		m_RenderAccumulator += timeslice;

		ZenMapFogClientData clientData = ZenMapFogClientData.Get();
		clientData.RequestSyncIfNeeded();
		if (clientData.IsReady() && !clientData.IsEnabled())
		{
			HideWidgets(m_FogWidgets, 0);
			HideWidgets(m_RoundedWidgets, 0);
			HideWidgets(m_GridWidgets, 0);
			return;
		}

		float scale = m_MapWidget.GetScale();
		vector mapPosition = m_MapWidget.GetMapPos();
		int revision = clientData.GetRevision();
		bool scaleChanged = Math.AbsFloat(scale - m_LastScale) > 0.00001;
		bool positionChanged = Math.AbsFloat(mapPosition[0] - m_LastMapPosition[0]) > 0.01 || Math.AbsFloat(mapPosition[2] - m_LastMapPosition[2]) > 0.01;
		bool revisionChanged = revision != m_LastRevision;

		if (!scaleChanged && !positionChanged && !revisionChanged)
			return;

		// Discovery changes and zoom changes refresh immediately. Pure map dragging is
		// capped at roughly 60 Hz so the root-level fog follows the MapWidget smoothly
		// without doing more GUI rebuild work than a normal display can meaningfully show.
		if (positionChanged && !scaleChanged && !revisionChanged && m_RenderAccumulator < ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS)
			return;

		if (m_RenderAccumulator >= ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS)
		{
			m_RenderAccumulator -= ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS;

			if (m_RenderAccumulator > ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS)
				m_RenderAccumulator = ZenMapFogConstants.CLIENT_RENDER_INTERVAL_SECONDS;
		}
		else
		{
			m_RenderAccumulator = 0.0;
		}

		m_LastScale = scale;
		m_LastMapPosition = mapPosition;
		m_LastRevision = revision;
		Refresh(clientData);
	}

	protected void Refresh(ZenMapFogClientData clientData)
	{
		float mapScreenX;
		float mapScreenY;
		float mapScreenWidth;
		float mapScreenHeight;
		m_MapWidget.GetScreenPos(mapScreenX, mapScreenY);
		m_MapWidget.GetScreenSize(mapScreenWidth, mapScreenHeight);

		if (mapScreenWidth <= 0.0 || mapScreenHeight <= 0.0)
			return;

		if (clientData.IsReady() && clientData.IsAllRevealed())
		{
			HideWidgets(m_FogWidgets, 0);
			HideWidgets(m_RoundedWidgets, 0);
			HideWidgets(m_GridWidgets, 0);
			return;
		}

		float mapRight = mapScreenX + mapScreenWidth;
		float mapBottom = mapScreenY + mapScreenHeight;
		vector worldTopLeft = m_MapWidget.ScreenToMap(Vector(mapScreenX, mapScreenY, 0));
		vector worldTopRight = m_MapWidget.ScreenToMap(Vector(mapRight, mapScreenY, 0));
		vector worldBottomLeft = m_MapWidget.ScreenToMap(Vector(mapScreenX, mapBottom, 0));
		vector worldBottomRight = m_MapWidget.ScreenToMap(Vector(mapRight, mapBottom, 0));

		float minWorldX = Math.Min(Math.Min(worldTopLeft[0], worldTopRight[0]), Math.Min(worldBottomLeft[0], worldBottomRight[0]));
		float maxWorldX = Math.Max(Math.Max(worldTopLeft[0], worldTopRight[0]), Math.Max(worldBottomLeft[0], worldBottomRight[0]));
		float minWorldZ = Math.Min(Math.Min(worldTopLeft[2], worldTopRight[2]), Math.Min(worldBottomLeft[2], worldBottomRight[2]));
		float maxWorldZ = Math.Max(Math.Max(worldTopLeft[2], worldTopRight[2]), Math.Max(worldBottomLeft[2], worldBottomRight[2]));

		// Keep the proven nominal-map renderer completely isolated from outside-terrain cells.
		// The transform below is only used by the separate outside-terrain pass.
		BuildOutsideTransform(clientData);

		int nominalFogWidgetCount = RenderFog(clientData, minWorldX, maxWorldX, minWorldZ, maxWorldZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
		int fogWidgetCount = RenderOutsideFog(clientData, nominalFogWidgetCount, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
		HideWidgets(m_FogWidgets, fogWidgetCount);

		int nominalRoundedWidgetCount = RenderRoundedEdges(clientData, minWorldX, maxWorldX, minWorldZ, maxWorldZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
		int roundedWidgetCount = RenderOutsideRoundedEdges(clientData, nominalRoundedWidgetCount, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
		HideWidgets(m_RoundedWidgets, roundedWidgetCount);

		if (fogWidgetCount > 0 || roundedWidgetCount > 0)
		{
			int gridWidgetCount = RenderGrid(clientData, minWorldX, maxWorldX, minWorldZ, maxWorldZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
			HideWidgets(m_GridWidgets, gridWidgetCount);
		}
		else
		{
			HideWidgets(m_GridWidgets, 0);
		}
	}


	protected int RenderFog(ZenMapFogClientData clientData, float minWorldX, float maxWorldX, float minWorldZ, float maxWorldZ, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		float cellSize = clientData.GetCellSizeMeters();
		if (cellSize <= 0.0)
			return 0;

		float originX = clientData.GetMapOriginX();
		float originZ = clientData.GetMapOriginZ();
		int columns = clientData.GetCellColumns();
		int rows = clientData.GetCellRows();
		int minCellX = Math.Floor((minWorldX - originX) / cellSize) - 1;
		int maxCellX = Math.Floor((maxWorldX - originX) / cellSize) + 1;
		int minCellZ = Math.Floor((minWorldZ - originZ) / cellSize) - 1;
		int maxCellZ = Math.Floor((maxWorldZ - originZ) / cellSize) + 1;

		minCellX = Math.Max(minCellX, 0);
		minCellZ = Math.Max(minCellZ, 0);
		maxCellX = Math.Min(maxCellX, columns - 1);
		maxCellZ = Math.Min(maxCellZ, rows - 1);

		if (maxCellX < minCellX || maxCellZ < minCellZ)
			return 0;

		bool syncReady = clientData.IsReady();
		int widgetIndex = 0;

		for (int cellZ = minCellZ; cellZ <= maxCellZ; cellZ++)
		{
			int runStartX = -1;
			int runFogType = FOG_TYPE_NONE;

			for (int cellX = minCellX; cellX <= maxCellX; cellX++)
			{
				int fogType = GetFogType(clientData, cellX, cellZ, syncReady);

				// Rounded cutout cells are rendered separately so their corner can actually be removed.
				if (fogType == FOG_TYPE_ROUNDED_CUTOUT || fogType == FOG_TYPE_BOUNDARY_ROUNDED_CUTOUT)
					fogType = FOG_TYPE_NONE;

				if (fogType == runFogType)
					continue;

				if (runStartX != -1)
				{
					if (PositionFogRun(widgetIndex, runStartX, cellX - 1, cellZ, runFogType, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight))
						widgetIndex++;
				}

				if (fogType != FOG_TYPE_NONE)
					runStartX = cellX;
				else
					runStartX = -1;

				runFogType = fogType;
			}

			if (runStartX != -1)
			{
				if (PositionFogRun(widgetIndex, runStartX, maxCellX, cellZ, runFogType, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight))
					widgetIndex++;
			}
		}

		return widgetIndex;
	}


	protected bool BuildOutsideTransform(ZenMapFogClientData clientData)
	{
		m_OutsideTransformValid = false;

		if (!clientData)
			return false;

		float transformCellSize = clientData.GetCellSizeMeters();
		int transformColumns = clientData.GetCellColumns();
		int transformRows = clientData.GetCellRows();
		if (transformCellSize <= 0.0 || transformColumns < 2 || transformRows < 2)
			return false;

		float transformOriginX = clientData.GetMapOriginX();
		float transformOriginZ = clientData.GetMapOriginZ();
		vector transformMapPosition = m_MapWidget.GetMapPos();

		int transformReferenceCellX = Math.Floor((transformMapPosition[0] - transformOriginX) / transformCellSize);
		int transformReferenceCellZ = Math.Floor((transformMapPosition[2] - transformOriginZ) / transformCellSize);
		transformReferenceCellX = Math.Max(0, Math.Min(transformReferenceCellX, transformColumns - 1));
		transformReferenceCellZ = Math.Max(0, Math.Min(transformReferenceCellZ, transformRows - 1));

		int transformProbeCellX = transformReferenceCellX + 1;
		int transformProbeDirectionX = 1;
		if (transformProbeCellX >= transformColumns)
		{
			transformProbeCellX = transformReferenceCellX - 1;
			transformProbeDirectionX = -1;
		}

		int transformProbeCellZ = transformReferenceCellZ + 1;
		int transformProbeDirectionZ = 1;
		if (transformProbeCellZ >= transformRows)
		{
			transformProbeCellZ = transformReferenceCellZ - 1;
			transformProbeDirectionZ = -1;
		}

		if (transformProbeCellX < 0 || transformProbeCellZ < 0)
			return false;

		float transformReferenceWorldX = transformOriginX + (transformReferenceCellX * transformCellSize);
		float transformReferenceWorldZ = transformOriginZ + (transformReferenceCellZ * transformCellSize);
		float transformProbeWorldX = transformOriginX + (transformProbeCellX * transformCellSize);
		float transformProbeWorldZ = transformOriginZ + (transformProbeCellZ * transformCellSize);

		vector transformReferenceScreen = m_MapWidget.MapToScreen(Vector(transformReferenceWorldX, 0, transformReferenceWorldZ));
		vector transformProbeXScreen = m_MapWidget.MapToScreen(Vector(transformProbeWorldX, 0, transformReferenceWorldZ));
		vector transformProbeZScreen = m_MapWidget.MapToScreen(Vector(transformReferenceWorldX, 0, transformProbeWorldZ));

		float transformStepXX = (transformProbeXScreen[0] - transformReferenceScreen[0]) / transformProbeDirectionX;
		float transformStepXY = (transformProbeXScreen[1] - transformReferenceScreen[1]) / transformProbeDirectionX;
		float transformStepZX = (transformProbeZScreen[0] - transformReferenceScreen[0]) / transformProbeDirectionZ;
		float transformStepZY = (transformProbeZScreen[1] - transformReferenceScreen[1]) / transformProbeDirectionZ;
		float transformDeterminant = (transformStepXX * transformStepZY) - (transformStepXY * transformStepZX);

		if (Math.AbsFloat(transformDeterminant) <= 0.0001)
			return false;

		m_OutsideReferenceCellX = transformReferenceCellX;
		m_OutsideReferenceCellZ = transformReferenceCellZ;
		m_OutsideReferenceScreen = transformReferenceScreen;
		m_OutsideStepX = Vector(transformStepXX, transformStepXY, 0);
		m_OutsideStepZ = Vector(transformStepZX, transformStepZY, 0);
		m_OutsideTransformValid = true;
		return true;
	}

	protected bool ScreenToOutsideCellCoords(float screenX, float screenY, out float cellX, out float cellZ)
	{
		if (!m_OutsideTransformValid)
			return false;

		float coordinateDeltaX = screenX - m_OutsideReferenceScreen[0];
		float coordinateDeltaY = screenY - m_OutsideReferenceScreen[1];
		float coordinateDeterminant = (m_OutsideStepX[0] * m_OutsideStepZ[1]) - (m_OutsideStepX[1] * m_OutsideStepZ[0]);

		if (Math.AbsFloat(coordinateDeterminant) <= 0.0001)
			return false;

		float coordinateOffsetX = ((coordinateDeltaX * m_OutsideStepZ[1]) - (coordinateDeltaY * m_OutsideStepZ[0])) / coordinateDeterminant;
		float coordinateOffsetZ = ((m_OutsideStepX[0] * coordinateDeltaY) - (m_OutsideStepX[1] * coordinateDeltaX)) / coordinateDeterminant;

		cellX = m_OutsideReferenceCellX + coordinateOffsetX;
		cellZ = m_OutsideReferenceCellZ + coordinateOffsetZ;
		return true;
	}

	protected bool GetOutsideVisibleCellBounds(float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight, out int minCellX, out int maxCellX, out int minCellZ, out int maxCellZ)
	{
		if (!m_OutsideTransformValid)
			return false;

		float boundsRight = mapScreenX + mapScreenWidth;
		float boundsBottom = mapScreenY + mapScreenHeight;

		float boundsTopLeftX;
		float boundsTopLeftZ;
		float boundsTopRightX;
		float boundsTopRightZ;
		float boundsBottomLeftX;
		float boundsBottomLeftZ;
		float boundsBottomRightX;
		float boundsBottomRightZ;

		if (!ScreenToOutsideCellCoords(mapScreenX, mapScreenY, boundsTopLeftX, boundsTopLeftZ))
			return false;
		if (!ScreenToOutsideCellCoords(boundsRight, mapScreenY, boundsTopRightX, boundsTopRightZ))
			return false;
		if (!ScreenToOutsideCellCoords(mapScreenX, boundsBottom, boundsBottomLeftX, boundsBottomLeftZ))
			return false;
		if (!ScreenToOutsideCellCoords(boundsRight, boundsBottom, boundsBottomRightX, boundsBottomRightZ))
			return false;

		float boundsMinimumX = Math.Min(Math.Min(boundsTopLeftX, boundsTopRightX), Math.Min(boundsBottomLeftX, boundsBottomRightX));
		float boundsMaximumX = Math.Max(Math.Max(boundsTopLeftX, boundsTopRightX), Math.Max(boundsBottomLeftX, boundsBottomRightX));
		float boundsMinimumZ = Math.Min(Math.Min(boundsTopLeftZ, boundsTopRightZ), Math.Min(boundsBottomLeftZ, boundsBottomRightZ));
		float boundsMaximumZ = Math.Max(Math.Max(boundsTopLeftZ, boundsTopRightZ), Math.Max(boundsBottomLeftZ, boundsBottomRightZ));

		minCellX = Math.Floor(boundsMinimumX) - 1;
		maxCellX = Math.Floor(boundsMaximumX) + 1;
		minCellZ = Math.Floor(boundsMinimumZ) - 1;
		maxCellZ = Math.Floor(boundsMaximumZ) + 1;

		minCellX = Math.Max(minCellX, ZenMapFogConstants.CELL_COORD_MIN);
		maxCellX = Math.Min(maxCellX, ZenMapFogConstants.CELL_COORD_MAX);
		minCellZ = Math.Max(minCellZ, ZenMapFogConstants.CELL_COORD_MIN);
		maxCellZ = Math.Min(maxCellZ, ZenMapFogConstants.CELL_COORD_MAX);

		return maxCellX >= minCellX && maxCellZ >= minCellZ;
	}

	protected vector OutsideCellCornerToScreen(int cellX, int cellZ)
	{
		float cornerOffsetX = cellX - m_OutsideReferenceCellX;
		float cornerOffsetZ = cellZ - m_OutsideReferenceCellZ;
		float cornerScreenX = m_OutsideReferenceScreen[0] + (cornerOffsetX * m_OutsideStepX[0]) + (cornerOffsetZ * m_OutsideStepZ[0]);
		float cornerScreenY = m_OutsideReferenceScreen[1] + (cornerOffsetX * m_OutsideStepX[1]) + (cornerOffsetZ * m_OutsideStepZ[1]);
		return Vector(cornerScreenX, cornerScreenY, 0);
	}

	protected bool GetOutsideCellScreenRect(int cellX, int cellZ, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight, out float left, out float top, out float right, out float bottom)
	{
		if (!m_OutsideTransformValid)
			return false;

		vector outsideCorner00 = OutsideCellCornerToScreen(cellX, cellZ);
		vector outsideCorner10 = OutsideCellCornerToScreen(cellX + 1, cellZ);
		vector outsideCorner01 = OutsideCellCornerToScreen(cellX, cellZ + 1);
		vector outsideCorner11 = OutsideCellCornerToScreen(cellX + 1, cellZ + 1);

		left = Math.Min(Math.Min(outsideCorner00[0], outsideCorner10[0]), Math.Min(outsideCorner01[0], outsideCorner11[0]));
		right = Math.Max(Math.Max(outsideCorner00[0], outsideCorner10[0]), Math.Max(outsideCorner01[0], outsideCorner11[0]));
		top = Math.Min(Math.Min(outsideCorner00[1], outsideCorner10[1]), Math.Min(outsideCorner01[1], outsideCorner11[1]));
		bottom = Math.Max(Math.Max(outsideCorner00[1], outsideCorner10[1]), Math.Max(outsideCorner01[1], outsideCorner11[1]));

		float outsideMapRight = mapScreenX + mapScreenWidth;
		float outsideMapBottom = mapScreenY + mapScreenHeight;

		left = Math.Max(left, mapScreenX);
		right = Math.Min(right, outsideMapRight);
		top = Math.Max(top, mapScreenY);
		bottom = Math.Min(bottom, outsideMapBottom);

		return right > left && bottom > top;
	}

	protected bool IsNominalCell(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		return cellX >= 0 && cellZ >= 0 && cellX < clientData.GetCellColumns() && cellZ < clientData.GetCellRows();
	}

	protected int RenderOutsideFog(ZenMapFogClientData clientData, int startWidgetIndex, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		if (!m_OutsideTransformValid)
			return startWidgetIndex;

		int outsideMinCellX;
		int outsideMaxCellX;
		int outsideMinCellZ;
		int outsideMaxCellZ;
		if (!GetOutsideVisibleCellBounds(mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideMinCellX, outsideMaxCellX, outsideMinCellZ, outsideMaxCellZ))
			return startWidgetIndex;

		int outsideColumns = clientData.GetCellColumns();
		int outsideRows = clientData.GetCellRows();
		bool outsideSyncReady = clientData.IsReady();
		int outsideWidgetIndex = startWidgetIndex;

		for (int outsideCellZ = outsideMinCellZ; outsideCellZ <= outsideMaxCellZ; outsideCellZ++)
		{
			if (outsideCellZ < 0 || outsideCellZ >= outsideRows)
			{
				outsideWidgetIndex = RenderOutsideFogRowSegment(clientData, outsideWidgetIndex, outsideMinCellX, outsideMaxCellX, outsideCellZ, outsideSyncReady, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
				continue;
			}

			if (outsideMinCellX < 0)
			{
				int outsideLeftEndCellX = Math.Min(outsideMaxCellX, -1);
				if (outsideLeftEndCellX >= outsideMinCellX)
					outsideWidgetIndex = RenderOutsideFogRowSegment(clientData, outsideWidgetIndex, outsideMinCellX, outsideLeftEndCellX, outsideCellZ, outsideSyncReady, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
			}

			if (outsideMaxCellX >= outsideColumns)
			{
				int outsideRightStartCellX = Math.Max(outsideMinCellX, outsideColumns);
				if (outsideRightStartCellX <= outsideMaxCellX)
					outsideWidgetIndex = RenderOutsideFogRowSegment(clientData, outsideWidgetIndex, outsideRightStartCellX, outsideMaxCellX, outsideCellZ, outsideSyncReady, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight);
			}
		}

		return outsideWidgetIndex;
	}

	protected int RenderOutsideFogRowSegment(ZenMapFogClientData clientData, int startWidgetIndex, int startCellX, int endCellX, int cellZ, bool syncReady, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		int segmentWidgetIndex = startWidgetIndex;
		int segmentRunStartX = -999999;
		int segmentRunFogType = FOG_TYPE_NONE;

		for (int segmentCellX = startCellX; segmentCellX <= endCellX; segmentCellX++)
		{
			int segmentFogType = GetFogType(clientData, segmentCellX, cellZ, syncReady);

			if (segmentFogType == FOG_TYPE_ROUNDED_CUTOUT || segmentFogType == FOG_TYPE_BOUNDARY_ROUNDED_CUTOUT)
				segmentFogType = FOG_TYPE_NONE;

			if (segmentFogType == segmentRunFogType)
				continue;

			if (segmentRunStartX != -999999)
			{
				if (PositionOutsideFogRun(segmentWidgetIndex, segmentRunStartX, segmentCellX - 1, cellZ, segmentRunFogType, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight))
					segmentWidgetIndex++;
			}

			if (segmentFogType != FOG_TYPE_NONE)
				segmentRunStartX = segmentCellX;
			else
				segmentRunStartX = -999999;

			segmentRunFogType = segmentFogType;
		}

		if (segmentRunStartX != -999999)
		{
			if (PositionOutsideFogRun(segmentWidgetIndex, segmentRunStartX, endCellX, cellZ, segmentRunFogType, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight))
				segmentWidgetIndex++;
		}

		return segmentWidgetIndex;
	}

	protected bool PositionOutsideFogRun(int widgetIndex, int startCellX, int endCellX, int cellZ, int fogType, ZenMapFogClientData clientData, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		if (!m_OutsideTransformValid)
			return false;

		vector outsideRunCorner00 = OutsideCellCornerToScreen(startCellX, cellZ);
		vector outsideRunCorner10 = OutsideCellCornerToScreen(endCellX + 1, cellZ);
		vector outsideRunCorner01 = OutsideCellCornerToScreen(startCellX, cellZ + 1);
		vector outsideRunCorner11 = OutsideCellCornerToScreen(endCellX + 1, cellZ + 1);

		float outsideRunLeft = Math.Min(Math.Min(outsideRunCorner00[0], outsideRunCorner10[0]), Math.Min(outsideRunCorner01[0], outsideRunCorner11[0]));
		float outsideRunRight = Math.Max(Math.Max(outsideRunCorner00[0], outsideRunCorner10[0]), Math.Max(outsideRunCorner01[0], outsideRunCorner11[0]));
		float outsideRunTop = Math.Min(Math.Min(outsideRunCorner00[1], outsideRunCorner10[1]), Math.Min(outsideRunCorner01[1], outsideRunCorner11[1]));
		float outsideRunBottom = Math.Max(Math.Max(outsideRunCorner00[1], outsideRunCorner10[1]), Math.Max(outsideRunCorner01[1], outsideRunCorner11[1]));
		float outsideRunMapRight = mapScreenX + mapScreenWidth;
		float outsideRunMapBottom = mapScreenY + mapScreenHeight;
		float outsideRunOverlap = clientData.GetTileOverlapPixels();

		outsideRunLeft = Math.Max(outsideRunLeft - outsideRunOverlap, mapScreenX);
		outsideRunRight = Math.Min(outsideRunRight + outsideRunOverlap, outsideRunMapRight);
		outsideRunTop = Math.Max(outsideRunTop - outsideRunOverlap, mapScreenY);
		outsideRunBottom = Math.Min(outsideRunBottom + outsideRunOverlap, outsideRunMapBottom);

		if (outsideRunRight <= outsideRunLeft || outsideRunBottom <= outsideRunTop)
			return false;

		Widget outsideFogWidget = GetOrCreateWidget(m_FogWidgets, widgetIndex, 500);
		if (!outsideFogWidget)
			return false;

		outsideFogWidget.SetScreenPos(outsideRunLeft, outsideRunTop);
		outsideFogWidget.SetScreenSize(outsideRunRight - outsideRunLeft, outsideRunBottom - outsideRunTop);

		if (fogType == FOG_TYPE_ADJACENT)
			outsideFogWidget.SetColor(clientData.GetAdjacentCellFogColor());
		else
			outsideFogWidget.SetColor(clientData.GetFogColor());

		outsideFogWidget.Show(true);
		return true;
	}

	protected int GetFogType(ZenMapFogClientData clientData, int cellX, int cellZ, bool syncReady)
	{
		if (!syncReady)
			return FOG_TYPE_FULL;

		int cellId = clientData.BuildCellId(cellX, cellZ);
		if (clientData.HasCell(cellId))
			return FOG_TYPE_NONE;

		if (clientData.GetRoundedFogEdgesEnabled() && clientData.GetRoundedFogEdgeRadiusPercent() > 0.0 && HasRoundedCutoutCorner(clientData, cellX, cellZ))
			return FOG_TYPE_ROUNDED_CUTOUT;

		if (clientData.GetRoundedFogEdgesBoundaryEnabled() && clientData.GetAdjacentCellFogEnabled() && clientData.GetRoundedFogEdgeRadiusPercent() > 0.0 && HasBoundaryRoundedCutoutCorner(clientData, cellX, cellZ))
			return FOG_TYPE_BOUNDARY_ROUNDED_CUTOUT;

		if (clientData.GetAdjacentCellFogEnabled() && clientData.IsAdjacentCell(cellId))
			return FOG_TYPE_ADJACENT;

		return FOG_TYPE_FULL;
	}

	protected bool PositionFogRun(int widgetIndex, int startCellX, int endCellX, int cellZ, int fogType, ZenMapFogClientData clientData, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		float cellSize = clientData.GetCellSizeMeters();
		float originX = clientData.GetMapOriginX();
		float originZ = clientData.GetMapOriginZ();
		float worldMinX = originX + (startCellX * cellSize);
		float worldMaxX = originX + ((endCellX + 1) * cellSize);
		float worldMinZ = originZ + (cellZ * cellSize);
		float worldMaxZ = worldMinZ + cellSize;
		vector screenA = m_MapWidget.MapToScreen(Vector(worldMinX, 0, worldMinZ));
		vector screenB = m_MapWidget.MapToScreen(Vector(worldMaxX, 0, worldMaxZ));

		float left = Math.Min(screenA[0], screenB[0]);
		float right = Math.Max(screenA[0], screenB[0]);
		float top = Math.Min(screenA[1], screenB[1]);
		float bottom = Math.Max(screenA[1], screenB[1]);
		float mapRight = mapScreenX + mapScreenWidth;
		float mapBottom = mapScreenY + mapScreenHeight;
		float overlap = clientData.GetTileOverlapPixels();

		left = Math.Max(left - overlap, mapScreenX);
		right = Math.Min(right + overlap, mapRight);
		top = Math.Max(top - overlap, mapScreenY);
		bottom = Math.Min(bottom + overlap, mapBottom);

		if (right <= left || bottom <= top)
			return false;

		Widget fogWidget = GetOrCreateWidget(m_FogWidgets, widgetIndex, 500);
		if (!fogWidget)
			return false;

		fogWidget.SetScreenPos(left, top);
		fogWidget.SetScreenSize(right - left, bottom - top);

		if (fogType == FOG_TYPE_ADJACENT)
			fogWidget.SetColor(clientData.GetAdjacentCellFogColor());
		else
			fogWidget.SetColor(clientData.GetFogColor());

		fogWidget.Show(true);
		return true;
	}

	protected int RenderRoundedEdges(ZenMapFogClientData clientData, float minWorldX, float maxWorldX, float minWorldZ, float maxWorldZ, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		if (!clientData.IsReady())
			return 0;

		bool innerRoundingEnabled = clientData.GetRoundedFogEdgesEnabled();
		bool boundaryRoundingEnabled = clientData.GetRoundedFogEdgesBoundaryEnabled() && clientData.GetAdjacentCellFogEnabled();
		if (!innerRoundingEnabled && !boundaryRoundingEnabled)
			return 0;

		float radiusPercent = clientData.GetRoundedFogEdgeRadiusPercent();
		if (radiusPercent <= 0.0)
			return 0;

		float cellSize = clientData.GetCellSizeMeters();
		if (cellSize <= 0.0)
			return 0;

		float originX = clientData.GetMapOriginX();
		float originZ = clientData.GetMapOriginZ();
		int minCellX = Math.Floor((minWorldX - originX) / cellSize) - 1;
		int maxCellX = Math.Floor((maxWorldX - originX) / cellSize) + 1;
		int minCellZ = Math.Floor((minWorldZ - originZ) / cellSize) - 1;
		int maxCellZ = Math.Floor((maxWorldZ - originZ) / cellSize) + 1;

		int columns = clientData.GetCellColumns();
		int rows = clientData.GetCellRows();
		minCellX = Math.Max(minCellX, 0);
		minCellZ = Math.Max(minCellZ, 0);
		maxCellX = Math.Min(maxCellX, columns - 1);
		maxCellZ = Math.Min(maxCellZ, rows - 1);

		if (maxCellX < minCellX || maxCellZ < minCellZ)
			return 0;

		float sampleWorldX = originX + ((minCellX + 0.5) * cellSize);
		float sampleWorldZ = originZ + ((minCellZ + 0.5) * cellSize);
		vector sampleScreen = m_MapWidget.MapToScreen(Vector(sampleWorldX, 0, sampleWorldZ));
		vector positiveXScreen = m_MapWidget.MapToScreen(Vector(sampleWorldX + cellSize, 0, sampleWorldZ));
		vector positiveZScreen = m_MapWidget.MapToScreen(Vector(sampleWorldX, 0, sampleWorldZ + cellSize));
		bool positiveXIsRight = positiveXScreen[0] > sampleScreen[0];
		bool positiveZIsDown = positiveZScreen[1] > sampleScreen[1];

		int widgetIndex = 0;

		if (innerRoundingEnabled)
		{
		// Render fog cells whose corner must be cut away. This handles concave/L-shaped boundaries.
		for (int fogCellZ = minCellZ; fogCellZ <= maxCellZ; fogCellZ++)
		{
			for (int fogCellX = minCellX; fogCellX <= maxCellX; fogCellX++)
			{
				int fogCellId = clientData.BuildCellId(fogCellX, fogCellZ);
				if (clientData.HasCell(fogCellId))
					continue;

				if (!HasRoundedCutoutCorner(clientData, fogCellX, fogCellZ))
					continue;

				bool clearXMinus = IsDiscoveredCell(clientData, fogCellX - 1, fogCellZ);
				bool clearXPlus = IsDiscoveredCell(clientData, fogCellX + 1, fogCellZ);
				bool clearZMinus = IsDiscoveredCell(clientData, fogCellX, fogCellZ - 1);
				bool clearZPlus = IsDiscoveredCell(clientData, fogCellX, fogCellZ + 1);

				bool clearLeft;
				bool clearRight;
				bool clearTop;
				bool clearBottom;

				if (positiveXIsRight)
				{
					clearLeft = clearXMinus;
					clearRight = clearXPlus;
				}
				else
				{
					clearLeft = clearXPlus;
					clearRight = clearXMinus;
				}

				if (positiveZIsDown)
				{
					clearTop = clearZMinus;
					clearBottom = clearZPlus;
				}
				else
				{
					clearTop = clearZPlus;
					clearBottom = clearZMinus;
				}

				bool cutTopLeft = clearTop && clearLeft;
				bool cutTopRight = clearTop && clearRight;
				bool cutBottomLeft = clearBottom && clearLeft;
				bool cutBottomRight = clearBottom && clearRight;

				float fogLeft;
				float fogTop;
				float fogRight;
				float fogBottom;
				if (!GetCellScreenRect(fogCellX, fogCellZ, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, fogLeft, fogTop, fogRight, fogBottom))
					continue;

				float fogCellWidth = fogRight - fogLeft;
				float fogCellHeight = fogBottom - fogTop;
				float fogRadius = Math.Min(fogCellWidth, fogCellHeight) * (radiusPercent / 100.0);

				int fogColor = clientData.GetFogColor();
				if (clientData.GetAdjacentCellFogEnabled() && clientData.IsAdjacentCell(fogCellId))
					fogColor = clientData.GetAdjacentCellFogColor();

				widgetIndex = PositionRoundedCutoutCell(widgetIndex, fogLeft, fogTop, fogRight, fogBottom, fogRadius, fogColor, cutTopLeft, cutTopRight, cutBottomLeft, cutBottomRight);
			}
		}

		// Soften convex corners by extending fog slightly into revealed cells.
		int roundedColor = clientData.GetFogColor();
		if (clientData.GetAdjacentCellFogEnabled())
			roundedColor = clientData.GetAdjacentCellFogColor();

		for (int cellZ = minCellZ; cellZ <= maxCellZ; cellZ++)
		{
			for (int cellX = minCellX; cellX <= maxCellX; cellX++)
			{
				int cellId = clientData.BuildCellId(cellX, cellZ);
				if (!clientData.HasCell(cellId))
					continue;

				bool fogXMinus = IsUndiscoveredCell(clientData, cellX - 1, cellZ);
				bool fogXPlus = IsUndiscoveredCell(clientData, cellX + 1, cellZ);
				bool fogZMinus = IsUndiscoveredCell(clientData, cellX, cellZ - 1);
				bool fogZPlus = IsUndiscoveredCell(clientData, cellX, cellZ + 1);

				bool leftFog;
				bool rightFog;
				bool topFog;
				bool bottomFog;

				if (positiveXIsRight)
				{
					leftFog = fogXMinus;
					rightFog = fogXPlus;
				}
				else
				{
					leftFog = fogXPlus;
					rightFog = fogXMinus;
				}

				if (positiveZIsDown)
				{
					topFog = fogZMinus;
					bottomFog = fogZPlus;
				}
				else
				{
					topFog = fogZPlus;
					bottomFog = fogZMinus;
				}

				if ((!topFog || !leftFog) && (!topFog || !rightFog) && (!bottomFog || !leftFog) && (!bottomFog || !rightFog))
					continue;

				float left;
				float top;
				float right;
				float bottom;
				if (!GetCellScreenRect(cellX, cellZ, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, left, top, right, bottom))
					continue;

				float cellWidth = right - left;
				float cellHeight = bottom - top;
				float radius = Math.Min(cellWidth, cellHeight) * (radiusPercent / 100.0);
				if (radius < 1.0)
					continue;

				if (topFog && leftFog)
					widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_TOP_LEFT, left, top, right, bottom, radius, roundedColor);

				if (topFog && rightFog)
					widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_TOP_RIGHT, left, top, right, bottom, radius, roundedColor);

				if (bottomFog && leftFog)
					widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_BOTTOM_LEFT, left, top, right, bottom, radius, roundedColor);

				if (bottomFog && rightFog)
					widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_BOTTOM_RIGHT, left, top, right, bottom, radius, roundedColor);
			}
		}


		}

		if (boundaryRoundingEnabled)
		{
			int fullFogColor = clientData.GetFogColor();
			int adjacentFogColor = clientData.GetAdjacentCellFogColor();

			// Round concave corners on the OUTER edge of the transparent adjacent-cell band.
			// Full-fog cells in an L of adjacent cells are drawn as a split full/adjacent cell,
			// so there is no transparent hole through to the actual map.
			for (int boundaryFogCellZ = minCellZ; boundaryFogCellZ <= maxCellZ; boundaryFogCellZ++)
			{
				for (int boundaryFogCellX = minCellX; boundaryFogCellX <= maxCellX; boundaryFogCellX++)
				{
					if (!HasBoundaryRoundedCutoutCorner(clientData, boundaryFogCellX, boundaryFogCellZ))
						continue;

					bool adjacentXMinus = IsAdjacentFogCell(clientData, boundaryFogCellX - 1, boundaryFogCellZ);
					bool adjacentXPlus = IsAdjacentFogCell(clientData, boundaryFogCellX + 1, boundaryFogCellZ);
					bool adjacentZMinus = IsAdjacentFogCell(clientData, boundaryFogCellX, boundaryFogCellZ - 1);
					bool adjacentZPlus = IsAdjacentFogCell(clientData, boundaryFogCellX, boundaryFogCellZ + 1);

					bool adjacentLeft;
					bool adjacentRight;
					bool adjacentTop;
					bool adjacentBottom;

					if (positiveXIsRight)
					{
						adjacentLeft = adjacentXMinus;
						adjacentRight = adjacentXPlus;
					}
					else
					{
						adjacentLeft = adjacentXPlus;
						adjacentRight = adjacentXMinus;
					}

					if (positiveZIsDown)
					{
						adjacentTop = adjacentZMinus;
						adjacentBottom = adjacentZPlus;
					}
					else
					{
						adjacentTop = adjacentZPlus;
						adjacentBottom = adjacentZMinus;
					}

					bool boundaryCutTopLeft = adjacentTop && adjacentLeft;
					bool boundaryCutTopRight = adjacentTop && adjacentRight;
					bool boundaryCutBottomLeft = adjacentBottom && adjacentLeft;
					bool boundaryCutBottomRight = adjacentBottom && adjacentRight;

					float boundaryLeft;
					float boundaryTop;
					float boundaryRight;
					float boundaryBottom;
					if (!GetCellScreenRect(boundaryFogCellX, boundaryFogCellZ, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, boundaryLeft, boundaryTop, boundaryRight, boundaryBottom))
						continue;

					float boundaryCellWidth = boundaryRight - boundaryLeft;
					float boundaryCellHeight = boundaryBottom - boundaryTop;
					float boundaryRadius = Math.Min(boundaryCellWidth, boundaryCellHeight) * (radiusPercent / 100.0);

					widgetIndex = PositionBoundaryRoundedCutoutCell(widgetIndex, boundaryLeft, boundaryTop, boundaryRight, boundaryBottom, boundaryRadius, fullFogColor, adjacentFogColor, boundaryCutTopLeft, boundaryCutTopRight, boundaryCutBottomLeft, boundaryCutBottomRight);
				}
			}

			// Round convex corners on the OUTER edge by extending full fog into the
			// adjacent-opacity cell where two full-fog sides meet.
			for (int adjacentCellZ = minCellZ; adjacentCellZ <= maxCellZ; adjacentCellZ++)
			{
				for (int adjacentCellX = minCellX; adjacentCellX <= maxCellX; adjacentCellX++)
				{
					if (!IsAdjacentFogCell(clientData, adjacentCellX, adjacentCellZ))
						continue;

					bool fullXMinus = IsFullFogCell(clientData, adjacentCellX - 1, adjacentCellZ);
					bool fullXPlus = IsFullFogCell(clientData, adjacentCellX + 1, adjacentCellZ);
					bool fullZMinus = IsFullFogCell(clientData, adjacentCellX, adjacentCellZ - 1);
					bool fullZPlus = IsFullFogCell(clientData, adjacentCellX, adjacentCellZ + 1);

					bool fullLeft;
					bool fullRight;
					bool fullTop;
					bool fullBottom;

					if (positiveXIsRight)
					{
						fullLeft = fullXMinus;
						fullRight = fullXPlus;
					}
					else
					{
						fullLeft = fullXPlus;
						fullRight = fullXMinus;
					}

					if (positiveZIsDown)
					{
						fullTop = fullZMinus;
						fullBottom = fullZPlus;
					}
					else
					{
						fullTop = fullZPlus;
						fullBottom = fullZMinus;
					}

					if ((!fullTop || !fullLeft) && (!fullTop || !fullRight) && (!fullBottom || !fullLeft) && (!fullBottom || !fullRight))
						continue;

					float adjacentScreenLeft;
					float adjacentScreenTop;
					float adjacentScreenRight;
					float adjacentScreenBottom;
					if (!GetCellScreenRect(adjacentCellX, adjacentCellZ, clientData, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, adjacentScreenLeft, adjacentScreenTop, adjacentScreenRight, adjacentScreenBottom))
						continue;

					float adjacentCellWidth = adjacentScreenRight - adjacentScreenLeft;
					float adjacentCellHeight = adjacentScreenBottom - adjacentScreenTop;
					float adjacentRadius = Math.Min(adjacentCellWidth, adjacentCellHeight) * (radiusPercent / 100.0);
					if (adjacentRadius < 1.0)
						continue;

					if (fullTop && fullLeft)
						widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_TOP_LEFT, adjacentScreenLeft, adjacentScreenTop, adjacentScreenRight, adjacentScreenBottom, adjacentRadius, fullFogColor);

					if (fullTop && fullRight)
						widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_TOP_RIGHT, adjacentScreenLeft, adjacentScreenTop, adjacentScreenRight, adjacentScreenBottom, adjacentRadius, fullFogColor);

					if (fullBottom && fullLeft)
						widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_BOTTOM_LEFT, adjacentScreenLeft, adjacentScreenTop, adjacentScreenRight, adjacentScreenBottom, adjacentRadius, fullFogColor);

					if (fullBottom && fullRight)
						widgetIndex = PositionRoundedCorner(widgetIndex, ROUNDED_CORNER_BOTTOM_RIGHT, adjacentScreenLeft, adjacentScreenTop, adjacentScreenRight, adjacentScreenBottom, adjacentRadius, fullFogColor);
				}
			}
		}

		return widgetIndex;
	}


	protected int RenderOutsideRoundedEdges(ZenMapFogClientData clientData, int startWidgetIndex, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		if (!clientData.IsReady() || !m_OutsideTransformValid)
			return startWidgetIndex;

		bool outsideInnerRoundingEnabled = clientData.GetRoundedFogEdgesEnabled();
		bool outsideBoundaryRoundingEnabled = clientData.GetRoundedFogEdgesBoundaryEnabled() && clientData.GetAdjacentCellFogEnabled();
		if (!outsideInnerRoundingEnabled && !outsideBoundaryRoundingEnabled)
			return startWidgetIndex;

		float outsideRadiusPercent = clientData.GetRoundedFogEdgeRadiusPercent();
		if (outsideRadiusPercent <= 0.0)
			return startWidgetIndex;

		int outsideRoundedMinCellX;
		int outsideRoundedMaxCellX;
		int outsideRoundedMinCellZ;
		int outsideRoundedMaxCellZ;
		if (!GetOutsideVisibleCellBounds(mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideRoundedMinCellX, outsideRoundedMaxCellX, outsideRoundedMinCellZ, outsideRoundedMaxCellZ))
			return startWidgetIndex;

		bool outsidePositiveXIsRight = m_OutsideStepX[0] > 0.0;
		bool outsidePositiveZIsDown = m_OutsideStepZ[1] > 0.0;
		int outsideRoundedWidgetIndex = startWidgetIndex;

		if (outsideInnerRoundingEnabled)
		{
			for (int outsideFogCellZ = outsideRoundedMinCellZ; outsideFogCellZ <= outsideRoundedMaxCellZ; outsideFogCellZ++)
			{
				for (int outsideFogCellX = outsideRoundedMinCellX; outsideFogCellX <= outsideRoundedMaxCellX; outsideFogCellX++)
				{
					if (IsNominalCell(clientData, outsideFogCellX, outsideFogCellZ))
						continue;

					int outsideFogCellId = clientData.BuildCellId(outsideFogCellX, outsideFogCellZ);
					if (clientData.HasCell(outsideFogCellId))
						continue;

					if (!HasRoundedCutoutCorner(clientData, outsideFogCellX, outsideFogCellZ))
						continue;

					bool outsideClearXMinus = IsDiscoveredCell(clientData, outsideFogCellX - 1, outsideFogCellZ);
					bool outsideClearXPlus = IsDiscoveredCell(clientData, outsideFogCellX + 1, outsideFogCellZ);
					bool outsideClearZMinus = IsDiscoveredCell(clientData, outsideFogCellX, outsideFogCellZ - 1);
					bool outsideClearZPlus = IsDiscoveredCell(clientData, outsideFogCellX, outsideFogCellZ + 1);

					bool outsideClearLeft;
					bool outsideClearRight;
					bool outsideClearTop;
					bool outsideClearBottom;

					if (outsidePositiveXIsRight)
					{
						outsideClearLeft = outsideClearXMinus;
						outsideClearRight = outsideClearXPlus;
					}
					else
					{
						outsideClearLeft = outsideClearXPlus;
						outsideClearRight = outsideClearXMinus;
					}

					if (outsidePositiveZIsDown)
					{
						outsideClearTop = outsideClearZMinus;
						outsideClearBottom = outsideClearZPlus;
					}
					else
					{
						outsideClearTop = outsideClearZPlus;
						outsideClearBottom = outsideClearZMinus;
					}

					bool outsideCutTopLeft = outsideClearTop && outsideClearLeft;
					bool outsideCutTopRight = outsideClearTop && outsideClearRight;
					bool outsideCutBottomLeft = outsideClearBottom && outsideClearLeft;
					bool outsideCutBottomRight = outsideClearBottom && outsideClearRight;

					float outsideFogLeft;
					float outsideFogTop;
					float outsideFogRight;
					float outsideFogBottom;
					if (!GetOutsideCellScreenRect(outsideFogCellX, outsideFogCellZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideFogLeft, outsideFogTop, outsideFogRight, outsideFogBottom))
						continue;

					float outsideFogCellWidth = outsideFogRight - outsideFogLeft;
					float outsideFogCellHeight = outsideFogBottom - outsideFogTop;
					float outsideFogRadius = Math.Min(outsideFogCellWidth, outsideFogCellHeight) * (outsideRadiusPercent / 100.0);

					int outsideFogColor = clientData.GetFogColor();
					if (clientData.GetAdjacentCellFogEnabled() && clientData.IsAdjacentCell(outsideFogCellId))
						outsideFogColor = clientData.GetAdjacentCellFogColor();

					outsideRoundedWidgetIndex = PositionRoundedCutoutCell(outsideRoundedWidgetIndex, outsideFogLeft, outsideFogTop, outsideFogRight, outsideFogBottom, outsideFogRadius, outsideFogColor, outsideCutTopLeft, outsideCutTopRight, outsideCutBottomLeft, outsideCutBottomRight);
				}
			}

			int outsideRoundedColor = clientData.GetFogColor();
			if (clientData.GetAdjacentCellFogEnabled())
				outsideRoundedColor = clientData.GetAdjacentCellFogColor();

			for (int outsideClearCellZ = outsideRoundedMinCellZ; outsideClearCellZ <= outsideRoundedMaxCellZ; outsideClearCellZ++)
			{
				for (int outsideClearCellX = outsideRoundedMinCellX; outsideClearCellX <= outsideRoundedMaxCellX; outsideClearCellX++)
				{
					if (IsNominalCell(clientData, outsideClearCellX, outsideClearCellZ))
						continue;

					int outsideClearCellId = clientData.BuildCellId(outsideClearCellX, outsideClearCellZ);
					if (!clientData.HasCell(outsideClearCellId))
						continue;

					bool outsideFogXMinus = IsUndiscoveredCell(clientData, outsideClearCellX - 1, outsideClearCellZ);
					bool outsideFogXPlus = IsUndiscoveredCell(clientData, outsideClearCellX + 1, outsideClearCellZ);
					bool outsideFogZMinus = IsUndiscoveredCell(clientData, outsideClearCellX, outsideClearCellZ - 1);
					bool outsideFogZPlus = IsUndiscoveredCell(clientData, outsideClearCellX, outsideClearCellZ + 1);

					bool outsideLeftFog;
					bool outsideRightFog;
					bool outsideTopFog;
					bool outsideBottomFog;

					if (outsidePositiveXIsRight)
					{
						outsideLeftFog = outsideFogXMinus;
						outsideRightFog = outsideFogXPlus;
					}
					else
					{
						outsideLeftFog = outsideFogXPlus;
						outsideRightFog = outsideFogXMinus;
					}

					if (outsidePositiveZIsDown)
					{
						outsideTopFog = outsideFogZMinus;
						outsideBottomFog = outsideFogZPlus;
					}
					else
					{
						outsideTopFog = outsideFogZPlus;
						outsideBottomFog = outsideFogZMinus;
					}

					if ((!outsideTopFog || !outsideLeftFog) && (!outsideTopFog || !outsideRightFog) && (!outsideBottomFog || !outsideLeftFog) && (!outsideBottomFog || !outsideRightFog))
						continue;

					float outsideClearScreenLeft;
					float outsideClearScreenTop;
					float outsideClearScreenRight;
					float outsideClearScreenBottom;
					if (!GetOutsideCellScreenRect(outsideClearCellX, outsideClearCellZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideClearScreenLeft, outsideClearScreenTop, outsideClearScreenRight, outsideClearScreenBottom))
						continue;

					float outsideClearCellWidth = outsideClearScreenRight - outsideClearScreenLeft;
					float outsideClearCellHeight = outsideClearScreenBottom - outsideClearScreenTop;
					float outsideClearRadius = Math.Min(outsideClearCellWidth, outsideClearCellHeight) * (outsideRadiusPercent / 100.0);
					if (outsideClearRadius < 1.0)
						continue;

					if (outsideTopFog && outsideLeftFog)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_TOP_LEFT, outsideClearScreenLeft, outsideClearScreenTop, outsideClearScreenRight, outsideClearScreenBottom, outsideClearRadius, outsideRoundedColor);

					if (outsideTopFog && outsideRightFog)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_TOP_RIGHT, outsideClearScreenLeft, outsideClearScreenTop, outsideClearScreenRight, outsideClearScreenBottom, outsideClearRadius, outsideRoundedColor);

					if (outsideBottomFog && outsideLeftFog)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_BOTTOM_LEFT, outsideClearScreenLeft, outsideClearScreenTop, outsideClearScreenRight, outsideClearScreenBottom, outsideClearRadius, outsideRoundedColor);

					if (outsideBottomFog && outsideRightFog)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_BOTTOM_RIGHT, outsideClearScreenLeft, outsideClearScreenTop, outsideClearScreenRight, outsideClearScreenBottom, outsideClearRadius, outsideRoundedColor);
				}
			}
		}

		if (outsideBoundaryRoundingEnabled)
		{
			int outsideFullFogColor = clientData.GetFogColor();
			int outsideAdjacentFogColor = clientData.GetAdjacentCellFogColor();

			for (int outsideBoundaryFogCellZ = outsideRoundedMinCellZ; outsideBoundaryFogCellZ <= outsideRoundedMaxCellZ; outsideBoundaryFogCellZ++)
			{
				for (int outsideBoundaryFogCellX = outsideRoundedMinCellX; outsideBoundaryFogCellX <= outsideRoundedMaxCellX; outsideBoundaryFogCellX++)
				{
					if (IsNominalCell(clientData, outsideBoundaryFogCellX, outsideBoundaryFogCellZ))
						continue;

					if (!HasBoundaryRoundedCutoutCorner(clientData, outsideBoundaryFogCellX, outsideBoundaryFogCellZ))
						continue;

					bool outsideAdjacentXMinus = IsAdjacentFogCell(clientData, outsideBoundaryFogCellX - 1, outsideBoundaryFogCellZ);
					bool outsideAdjacentXPlus = IsAdjacentFogCell(clientData, outsideBoundaryFogCellX + 1, outsideBoundaryFogCellZ);
					bool outsideAdjacentZMinus = IsAdjacentFogCell(clientData, outsideBoundaryFogCellX, outsideBoundaryFogCellZ - 1);
					bool outsideAdjacentZPlus = IsAdjacentFogCell(clientData, outsideBoundaryFogCellX, outsideBoundaryFogCellZ + 1);

					bool outsideAdjacentLeft;
					bool outsideAdjacentRight;
					bool outsideAdjacentTop;
					bool outsideAdjacentBottom;

					if (outsidePositiveXIsRight)
					{
						outsideAdjacentLeft = outsideAdjacentXMinus;
						outsideAdjacentRight = outsideAdjacentXPlus;
					}
					else
					{
						outsideAdjacentLeft = outsideAdjacentXPlus;
						outsideAdjacentRight = outsideAdjacentXMinus;
					}

					if (outsidePositiveZIsDown)
					{
						outsideAdjacentTop = outsideAdjacentZMinus;
						outsideAdjacentBottom = outsideAdjacentZPlus;
					}
					else
					{
						outsideAdjacentTop = outsideAdjacentZPlus;
						outsideAdjacentBottom = outsideAdjacentZMinus;
					}

					bool outsideBoundaryCutTopLeft = outsideAdjacentTop && outsideAdjacentLeft;
					bool outsideBoundaryCutTopRight = outsideAdjacentTop && outsideAdjacentRight;
					bool outsideBoundaryCutBottomLeft = outsideAdjacentBottom && outsideAdjacentLeft;
					bool outsideBoundaryCutBottomRight = outsideAdjacentBottom && outsideAdjacentRight;

					float outsideBoundaryLeft;
					float outsideBoundaryTop;
					float outsideBoundaryRight;
					float outsideBoundaryBottom;
					if (!GetOutsideCellScreenRect(outsideBoundaryFogCellX, outsideBoundaryFogCellZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideBoundaryLeft, outsideBoundaryTop, outsideBoundaryRight, outsideBoundaryBottom))
						continue;

					float outsideBoundaryCellWidth = outsideBoundaryRight - outsideBoundaryLeft;
					float outsideBoundaryCellHeight = outsideBoundaryBottom - outsideBoundaryTop;
					float outsideBoundaryRadius = Math.Min(outsideBoundaryCellWidth, outsideBoundaryCellHeight) * (outsideRadiusPercent / 100.0);

					outsideRoundedWidgetIndex = PositionBoundaryRoundedCutoutCell(outsideRoundedWidgetIndex, outsideBoundaryLeft, outsideBoundaryTop, outsideBoundaryRight, outsideBoundaryBottom, outsideBoundaryRadius, outsideFullFogColor, outsideAdjacentFogColor, outsideBoundaryCutTopLeft, outsideBoundaryCutTopRight, outsideBoundaryCutBottomLeft, outsideBoundaryCutBottomRight);
				}
			}

			for (int outsideAdjacentCellZ = outsideRoundedMinCellZ; outsideAdjacentCellZ <= outsideRoundedMaxCellZ; outsideAdjacentCellZ++)
			{
				for (int outsideAdjacentCellX = outsideRoundedMinCellX; outsideAdjacentCellX <= outsideRoundedMaxCellX; outsideAdjacentCellX++)
				{
					if (IsNominalCell(clientData, outsideAdjacentCellX, outsideAdjacentCellZ))
						continue;

					if (!IsAdjacentFogCell(clientData, outsideAdjacentCellX, outsideAdjacentCellZ))
						continue;

					bool outsideFullXMinus = IsFullFogCell(clientData, outsideAdjacentCellX - 1, outsideAdjacentCellZ);
					bool outsideFullXPlus = IsFullFogCell(clientData, outsideAdjacentCellX + 1, outsideAdjacentCellZ);
					bool outsideFullZMinus = IsFullFogCell(clientData, outsideAdjacentCellX, outsideAdjacentCellZ - 1);
					bool outsideFullZPlus = IsFullFogCell(clientData, outsideAdjacentCellX, outsideAdjacentCellZ + 1);

					bool outsideFullLeft;
					bool outsideFullRight;
					bool outsideFullTop;
					bool outsideFullBottom;

					if (outsidePositiveXIsRight)
					{
						outsideFullLeft = outsideFullXMinus;
						outsideFullRight = outsideFullXPlus;
					}
					else
					{
						outsideFullLeft = outsideFullXPlus;
						outsideFullRight = outsideFullXMinus;
					}

					if (outsidePositiveZIsDown)
					{
						outsideFullTop = outsideFullZMinus;
						outsideFullBottom = outsideFullZPlus;
					}
					else
					{
						outsideFullTop = outsideFullZPlus;
						outsideFullBottom = outsideFullZMinus;
					}

					if ((!outsideFullTop || !outsideFullLeft) && (!outsideFullTop || !outsideFullRight) && (!outsideFullBottom || !outsideFullLeft) && (!outsideFullBottom || !outsideFullRight))
						continue;

					float outsideAdjacentScreenLeft;
					float outsideAdjacentScreenTop;
					float outsideAdjacentScreenRight;
					float outsideAdjacentScreenBottom;
					if (!GetOutsideCellScreenRect(outsideAdjacentCellX, outsideAdjacentCellZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, outsideAdjacentScreenLeft, outsideAdjacentScreenTop, outsideAdjacentScreenRight, outsideAdjacentScreenBottom))
						continue;

					float outsideAdjacentCellWidth = outsideAdjacentScreenRight - outsideAdjacentScreenLeft;
					float outsideAdjacentCellHeight = outsideAdjacentScreenBottom - outsideAdjacentScreenTop;
					float outsideAdjacentRadius = Math.Min(outsideAdjacentCellWidth, outsideAdjacentCellHeight) * (outsideRadiusPercent / 100.0);
					if (outsideAdjacentRadius < 1.0)
						continue;

					if (outsideFullTop && outsideFullLeft)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_TOP_LEFT, outsideAdjacentScreenLeft, outsideAdjacentScreenTop, outsideAdjacentScreenRight, outsideAdjacentScreenBottom, outsideAdjacentRadius, outsideFullFogColor);

					if (outsideFullTop && outsideFullRight)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_TOP_RIGHT, outsideAdjacentScreenLeft, outsideAdjacentScreenTop, outsideAdjacentScreenRight, outsideAdjacentScreenBottom, outsideAdjacentRadius, outsideFullFogColor);

					if (outsideFullBottom && outsideFullLeft)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_BOTTOM_LEFT, outsideAdjacentScreenLeft, outsideAdjacentScreenTop, outsideAdjacentScreenRight, outsideAdjacentScreenBottom, outsideAdjacentRadius, outsideFullFogColor);

					if (outsideFullBottom && outsideFullRight)
						outsideRoundedWidgetIndex = PositionRoundedCorner(outsideRoundedWidgetIndex, ROUNDED_CORNER_BOTTOM_RIGHT, outsideAdjacentScreenLeft, outsideAdjacentScreenTop, outsideAdjacentScreenRight, outsideAdjacentScreenBottom, outsideAdjacentRadius, outsideFullFogColor);
				}
			}
		}

		return outsideRoundedWidgetIndex;
	}

	protected bool HasBoundaryRoundedCutoutCorner(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		if (!IsFullFogCell(clientData, cellX, cellZ))
			return false;

		bool adjacentXMinus = IsAdjacentFogCell(clientData, cellX - 1, cellZ);
		bool adjacentXPlus = IsAdjacentFogCell(clientData, cellX + 1, cellZ);
		bool adjacentZMinus = IsAdjacentFogCell(clientData, cellX, cellZ - 1);
		bool adjacentZPlus = IsAdjacentFogCell(clientData, cellX, cellZ + 1);

		if (adjacentXMinus && adjacentZMinus)
			return true;

		if (adjacentXMinus && adjacentZPlus)
			return true;

		if (adjacentXPlus && adjacentZMinus)
			return true;

		if (adjacentXPlus && adjacentZPlus)
			return true;

		return false;
	}

	protected bool IsAdjacentFogCell(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		if (!clientData.IsCellCoordinateSupported(cellX, cellZ))
			return false;

		int cellId = clientData.BuildCellId(cellX, cellZ);
		if (clientData.HasCell(cellId))
			return false;

		return clientData.IsAdjacentCell(cellId);
	}

	protected bool IsFullFogCell(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		if (!clientData.IsCellCoordinateSupported(cellX, cellZ))
			return false;

		int cellId = clientData.BuildCellId(cellX, cellZ);
		if (clientData.HasCell(cellId))
			return false;

		return !clientData.IsAdjacentCell(cellId);
	}

	protected bool HasRoundedCutoutCorner(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		bool clearXMinus = IsDiscoveredCell(clientData, cellX - 1, cellZ);
		bool clearXPlus = IsDiscoveredCell(clientData, cellX + 1, cellZ);
		bool clearZMinus = IsDiscoveredCell(clientData, cellX, cellZ - 1);
		bool clearZPlus = IsDiscoveredCell(clientData, cellX, cellZ + 1);

		if (clearXMinus && clearZMinus)
			return true;

		if (clearXMinus && clearZPlus)
			return true;

		if (clearXPlus && clearZMinus)
			return true;

		if (clearXPlus && clearZPlus)
			return true;

		return false;
	}

	protected bool IsDiscoveredCell(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		if (!clientData.IsCellCoordinateSupported(cellX, cellZ))
			return false;

		int cellId = clientData.BuildCellId(cellX, cellZ);
		return clientData.HasCell(cellId);
	}

	protected bool IsUndiscoveredCell(ZenMapFogClientData clientData, int cellX, int cellZ)
	{
		if (!clientData.IsCellCoordinateSupported(cellX, cellZ))
			return false;

		int cellId = clientData.BuildCellId(cellX, cellZ);
		return !clientData.HasCell(cellId);
	}

	protected bool GetCellScreenRect(int cellX, int cellZ, ZenMapFogClientData clientData, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight, out float left, out float top, out float right, out float bottom)
	{
		if (!IsNominalCell(clientData, cellX, cellZ))
			return GetOutsideCellScreenRect(cellX, cellZ, mapScreenX, mapScreenY, mapScreenWidth, mapScreenHeight, left, top, right, bottom);

		float cellSize = clientData.GetCellSizeMeters();
		float originX = clientData.GetMapOriginX();
		float originZ = clientData.GetMapOriginZ();
		float worldMinX = originX + (cellX * cellSize);
		float worldMaxX = worldMinX + cellSize;
		float worldMinZ = originZ + (cellZ * cellSize);
		float worldMaxZ = worldMinZ + cellSize;
		vector screenA = m_MapWidget.MapToScreen(Vector(worldMinX, 0, worldMinZ));
		vector screenB = m_MapWidget.MapToScreen(Vector(worldMaxX, 0, worldMaxZ));

		left = Math.Min(screenA[0], screenB[0]);
		right = Math.Max(screenA[0], screenB[0]);
		top = Math.Min(screenA[1], screenB[1]);
		bottom = Math.Max(screenA[1], screenB[1]);

		float mapRight = mapScreenX + mapScreenWidth;
		float mapBottom = mapScreenY + mapScreenHeight;

		left = Math.Max(left, mapScreenX);
		right = Math.Min(right, mapRight);
		top = Math.Max(top, mapScreenY);
		bottom = Math.Min(bottom, mapBottom);

		return right > left && bottom > top;
	}

	protected int PositionRoundedCutoutCell(int widgetIndex, float left, float top, float right, float bottom, float radius, int color, bool cutTopLeft, bool cutTopRight, bool cutBottomLeft, bool cutBottomRight)
	{
		if (radius < 1.0)
			return PositionRoundedStrip(widgetIndex, left, top, right, bottom, color);
		float cellHeight = bottom - top;
		float maximumRadius = cellHeight * 0.5;
		if (radius > maximumRadius)
			radius = maximumRadius;

		float stepHeight = radius / ROUNDED_EDGE_STEPS;
		if (stepHeight <= 0.0)
			return widgetIndex;

		for (int topStep = 0; topStep < ROUNDED_EDGE_STEPS; topStep++)
		{
			float topFraction = GetRoundedCornerWidthFraction(topStep);
			float topLeftInset = 0.0;
			float topRightInset = 0.0;

			if (cutTopLeft)
				topLeftInset = radius * topFraction;

			if (cutTopRight)
				topRightInset = radius * topFraction;

			float topStripTop = top + (topStep * stepHeight);
			widgetIndex = PositionRoundedStrip(widgetIndex, left + topLeftInset, topStripTop, right - topRightInset, topStripTop + stepHeight + 0.5, color);
		}

		float middleTop = top + radius;
		float middleBottom = bottom - radius;
		if (middleBottom > middleTop)
			widgetIndex = PositionRoundedStrip(widgetIndex, left, middleTop, right, middleBottom, color);

		for (int bottomStep = 0; bottomStep < ROUNDED_EDGE_STEPS; bottomStep++)
		{
			float bottomFraction = GetRoundedCornerWidthFraction(bottomStep);
			float bottomLeftInset = 0.0;
			float bottomRightInset = 0.0;

			if (cutBottomLeft)
				bottomLeftInset = radius * bottomFraction;

			if (cutBottomRight)
				bottomRightInset = radius * bottomFraction;

			float bottomStripBottom = bottom - (bottomStep * stepHeight);
			widgetIndex = PositionRoundedStrip(widgetIndex, left + bottomLeftInset, bottomStripBottom - stepHeight - 0.5, right - bottomRightInset, bottomStripBottom, color);
		}

		return widgetIndex;
	}


	protected int PositionBoundaryRoundedCutoutCell(int widgetIndex, float left, float top, float right, float bottom, float radius, int fullColor, int adjacentColor, bool cutTopLeft, bool cutTopRight, bool cutBottomLeft, bool cutBottomRight)
	{
		if (radius < 1.0)
			return PositionRoundedStrip(widgetIndex, left, top, right, bottom, fullColor);
		float cellHeight = bottom - top;
		float maximumRadius = cellHeight * 0.5;
		if (radius > maximumRadius)
			radius = maximumRadius;

		float stepHeight = radius / ROUNDED_EDGE_STEPS;
		if (stepHeight <= 0.0)
			return widgetIndex;

		for (int topStep = 0; topStep < ROUNDED_EDGE_STEPS; topStep++)
		{
			float topFraction = GetRoundedCornerWidthFraction(topStep);
			float topLeftInset = 0.0;
			float topRightInset = 0.0;

			if (cutTopLeft)
				topLeftInset = radius * topFraction;

			if (cutTopRight)
				topRightInset = radius * topFraction;

			float topStripTop = top + (topStep * stepHeight);
			float topStripBottom = topStripTop + stepHeight + 0.5;

			if (topLeftInset > 0.0)
				widgetIndex = PositionRoundedStrip(widgetIndex, left, topStripTop, left + topLeftInset, topStripBottom, adjacentColor);

			widgetIndex = PositionRoundedStrip(widgetIndex, left + topLeftInset, topStripTop, right - topRightInset, topStripBottom, fullColor);

			if (topRightInset > 0.0)
				widgetIndex = PositionRoundedStrip(widgetIndex, right - topRightInset, topStripTop, right, topStripBottom, adjacentColor);
		}

		float middleTop = top + radius;
		float middleBottom = bottom - radius;
		if (middleBottom > middleTop)
			widgetIndex = PositionRoundedStrip(widgetIndex, left, middleTop, right, middleBottom, fullColor);

		for (int bottomStep = 0; bottomStep < ROUNDED_EDGE_STEPS; bottomStep++)
		{
			float bottomFraction = GetRoundedCornerWidthFraction(bottomStep);
			float bottomLeftInset = 0.0;
			float bottomRightInset = 0.0;

			if (cutBottomLeft)
				bottomLeftInset = radius * bottomFraction;

			if (cutBottomRight)
				bottomRightInset = radius * bottomFraction;

			float bottomStripBottom = bottom - (bottomStep * stepHeight);
			float bottomStripTop = bottomStripBottom - stepHeight - 0.5;

			if (bottomLeftInset > 0.0)
				widgetIndex = PositionRoundedStrip(widgetIndex, left, bottomStripTop, left + bottomLeftInset, bottomStripBottom, adjacentColor);

			widgetIndex = PositionRoundedStrip(widgetIndex, left + bottomLeftInset, bottomStripTop, right - bottomRightInset, bottomStripBottom, fullColor);

			if (bottomRightInset > 0.0)
				widgetIndex = PositionRoundedStrip(widgetIndex, right - bottomRightInset, bottomStripTop, right, bottomStripBottom, adjacentColor);
		}

		return widgetIndex;
	}

	protected int PositionRoundedStrip(int widgetIndex, float left, float top, float right, float bottom, int color)
	{
		if (right <= left || bottom <= top)
			return widgetIndex;

		Widget roundedWidget = GetOrCreateWidget(m_RoundedWidgets, widgetIndex, 505);
		if (!roundedWidget)
			return widgetIndex;

		roundedWidget.SetScreenPos(left, top);
		roundedWidget.SetScreenSize(right - left, bottom - top);
		roundedWidget.SetColor(color);
		roundedWidget.Show(true);
		return widgetIndex + 1;
	}

	protected int PositionRoundedCorner(int widgetIndex, int cornerType, float left, float top, float right, float bottom, float radius, int color)
	{
		float stepHeight = radius / ROUNDED_EDGE_STEPS;
		if (stepHeight <= 0.0)
			return widgetIndex;

		for (int step = 0; step < ROUNDED_EDGE_STEPS; step++)
		{
			float widthFraction = GetRoundedCornerWidthFraction(step);
			float stripWidth = radius * widthFraction;
			if (stripWidth < 0.5)
				continue;

			float stripX = left;
			float stripY = top + (step * stepHeight);

			if (cornerType == ROUNDED_CORNER_TOP_RIGHT || cornerType == ROUNDED_CORNER_BOTTOM_RIGHT)
				stripX = right - stripWidth;

			if (cornerType == ROUNDED_CORNER_BOTTOM_LEFT || cornerType == ROUNDED_CORNER_BOTTOM_RIGHT)
				stripY = bottom - ((step + 1) * stepHeight);

			Widget roundedWidget = GetOrCreateWidget(m_RoundedWidgets, widgetIndex, 505);
			if (!roundedWidget)
				continue;

			roundedWidget.SetScreenPos(stripX, stripY);
			roundedWidget.SetScreenSize(stripWidth, stepHeight + 0.5);
			roundedWidget.SetColor(color);
			roundedWidget.Show(true);
			widgetIndex++;
		}

		return widgetIndex;
	}

	protected float GetRoundedCornerWidthFraction(int step)
	{
		if (step <= 0)
			return 1.0;

		if (step == 1)
			return 0.6003;

		if (step == 2)
			return 0.4472;

		if (step == 3)
			return 0.3386;

		if (step == 4)
			return 0.2546;

		if (step == 5)
			return 0.1878;

		if (step == 6)
			return 0.1340;

		if (step == 7)
			return 0.0901;

		if (step == 8)
			return 0.0572;

		if (step == 9)
			return 0.0318;

		if (step == 10)
			return 0.0140;

		return 0.0035;
	}

	protected int RenderGrid(ZenMapFogClientData clientData, float minWorldX, float maxWorldX, float minWorldZ, float maxWorldZ, float mapScreenX, float mapScreenY, float mapScreenWidth, float mapScreenHeight)
	{
		if (!clientData.GetGridLinesEnabled())
			return 0;

		float gridSize = clientData.GetGridCellSizeMeters();
		float lineWidth = clientData.GetGridLineWidthPixels();
		if (gridSize <= 0.0 || lineWidth <= 0.0)
			return 0;

		float originX = clientData.GetMapOriginX();
		float originZ = clientData.GetMapOriginZ();
		int color = clientData.GetGridLineColor();
		vector mapCenter = m_MapWidget.GetMapPos();
		int minGridX = Math.Floor((minWorldX - originX) / gridSize);
		int maxGridX = Math.Ceil((maxWorldX - originX) / gridSize);
		int minGridZ = Math.Floor((minWorldZ - originZ) / gridSize);
		int maxGridZ = Math.Ceil((maxWorldZ - originZ) / gridSize);
		int widgetIndex = 0;

		for (int gridX = minGridX; gridX <= maxGridX; gridX++)
		{
			float worldX = originX + (gridX * gridSize);
			vector screenX = m_MapWidget.MapToScreen(Vector(worldX, 0, mapCenter[2]));
			float x = screenX[0];
			if (x < mapScreenX || x > mapScreenX + mapScreenWidth)
				continue;

			Widget verticalLine = GetOrCreateWidget(m_GridWidgets, widgetIndex, 510);
			if (!verticalLine)
				continue;

			verticalLine.SetScreenPos(x - (lineWidth * 0.5), mapScreenY);
			verticalLine.SetScreenSize(lineWidth, mapScreenHeight);
			verticalLine.SetColor(color);
			verticalLine.Show(true);
			widgetIndex++;
		}

		for (int gridZ = minGridZ; gridZ <= maxGridZ; gridZ++)
		{
			float worldZ = originZ + (gridZ * gridSize);
			vector screenZ = m_MapWidget.MapToScreen(Vector(mapCenter[0], 0, worldZ));
			float y = screenZ[1];
			if (y < mapScreenY || y > mapScreenY + mapScreenHeight)
				continue;

			Widget horizontalLine = GetOrCreateWidget(m_GridWidgets, widgetIndex, 510);
			if (!horizontalLine)
				continue;

			horizontalLine.SetScreenPos(mapScreenX, y - (lineWidth * 0.5));
			horizontalLine.SetScreenSize(mapScreenWidth, lineWidth);
			horizontalLine.SetColor(color);
			horizontalLine.Show(true);
			widgetIndex++;
		}

		return widgetIndex;
	}

	protected Widget GetOrCreateWidget(array<Widget> widgets, int index, int sortValue)
	{
		if (!widgets || index < 0)
			return null;

		if (index < widgets.Count())
		{
			Widget existingWidget = widgets[index];
			if (existingWidget)
			{
				if (existingWidget.GetSort() != sortValue)
					existingWidget.SetSort(sortValue);

				return existingWidget;
			}
		}

		Widget newWidget = g_Game.GetWorkspace().CreateWidgets("ZenMapFog/GUI/layouts/zen_map_fog_rect.layout", m_MenuRoot);
		if (!newWidget)
		{
			if (!m_CreateFailureLogged)
			{
				m_CreateFailureLogged = true;
				Print("[ZenMapFog] ERROR: Failed to create root-level fog rectangle widget.");
			}

			return null;
		}

		newWidget.SetSort(sortValue);
		widgets.Insert(newWidget);
		return newWidget;
	}

	protected void HideWidgets(array<Widget> widgets, int startIndex)
	{
		if (!widgets)
			return;

		for (int i = startIndex; i < widgets.Count(); i++)
		{
			if (widgets[i])
				widgets[i].Show(false);
		}
	}

	protected void UnlinkWidgets(array<Widget> widgets)
	{
		if (!widgets)
			return;

		foreach (Widget widget : widgets)
		{
			if (widget)
				widget.Unlink();
		}

		widgets.Clear();
	}
};
