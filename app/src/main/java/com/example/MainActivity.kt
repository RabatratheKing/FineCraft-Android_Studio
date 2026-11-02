package com.example

import android.content.res.AssetManager
import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.*
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.ui.theme.*
import kotlin.math.roundToInt
import kotlin.math.sqrt

enum 
class AppState {
    MAIN_MENU,
    SETTINGS,
    GAMEPLAY
}

class MainActivity : ComponentActivity() {
  private var currentAppState by mutableStateOf(AppState.MAIN_MENU)
  lateinit var settings: SettingsManager
  
  override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        try {
            val inputStream = assets.open("atlas.png")
            val bmp = android.graphics.BitmapFactory.decodeStream(inputStream)
            if (bmp != null) {
                loadedAtlasBitmap = bmp.asImageBitmap()
                val width = bmp.width
                val height = bmp.height
                val pixels = IntArray(width * height)
                bmp.getPixels(pixels, 0, width, 0, 0, width, height)
                nativeSetAtlasPixels(pixels, width, height)
            }
        } catch (e: Exception) {
            e.printStackTrace()
            // Fallback to C++ procedural generation
        }

    settings = SettingsManager(this)
    settings.updateNative()
    MainActivity.nativeInit(this.assets, this.filesDir.absolutePath, this.cacheDir.absolutePath)
    MainActivity.nativeInitSave(this.filesDir.absolutePath)
    
    enableEdgeToEdge()
    setContent {
      MyApplicationTheme {
        androidx.compose.runtime.LaunchedEffect(Unit) {
                    }
        when (currentAppState) {
            AppState.MAIN_MENU -> MainMenu(
                onContinueClick = { 
                    MainActivity.nativeSetDebugWorld(false)
                    MainActivity.nativeLoadGame()
                    currentAppState = AppState.GAMEPLAY 
                },
                onNewGameClick = { 
                    MainActivity.nativeSetDebugWorld(false)
                    MainActivity.nativeNewGame()
                    currentAppState = AppState.GAMEPLAY 
                },
                onDebugWorldClick = {
                    MainActivity.nativeSetDebugWorld(true)
                    MainActivity.nativeNewGame()
                    currentAppState = AppState.GAMEPLAY
                },
                onSettingsClick = { currentAppState = AppState.SETTINGS }
            )
            AppState.SETTINGS -> SettingsMenu(settings, onBack = { currentAppState = AppState.MAIN_MENU })
            AppState.GAMEPLAY -> GameplayScreen(settings, onBack = { 
                MainActivity.nativeSaveGame()
                currentAppState = AppState.MAIN_MENU 
            })
        }
        
        
      }
    }
  }

    override fun onPause() {
    super.onPause()
    if (currentAppState == AppState.GAMEPLAY) {
        MainActivity.nativeSaveGame()
    }
  }

  override fun onDestroy() {
    super.onDestroy()
      }

  external fun stringFromJNI(): String

  companion object {
    var loadedAtlasBitmap: ImageBitmap? = null

    init {
      System.loadLibrary("finecraft")
    }
    
    @JvmStatic external fun nativeSurfaceCreated(surface: Surface)
    @JvmStatic external fun nativeSurfaceChanged(width: Int, height: Int)
    @JvmStatic external fun nativeSurfaceDestroyed()
    
    @JvmStatic external fun nativeSetAtlasPixels(pixels: IntArray, width: Int, height: Int)
    @JvmStatic external fun nativeIsUsingExternalAtlas(): Boolean
    @JvmStatic
    external fun nativeGetItemSprite(itemId: Int): Int
    @JvmStatic external fun nativeGetAtlasPixels(): IntArray?

    @JvmStatic external fun nativeMoveItems(srcType: Int, srcSlot: Int, destType: Int, destSlot: Int, amount: Int): Boolean
    
    @JvmStatic external fun nativeCameraLook(dx: Float, dy: Float)
    @JvmStatic external fun nativeMoveJoystick(x: Float, y: Float)
    @JvmStatic external fun nativeAction(action: String)
    @JvmStatic external fun nativeSetDebugWorld(isDebug: Boolean)
    @JvmStatic external fun nativeGetAtlasWidth(): Int
    @JvmStatic external fun nativeGetAtlasHeight(): Int
    
    @JvmStatic external fun nativeGetInventory(): IntArray
    @JvmStatic external fun nativeSwapSlots(slotA: Int, slotB: Int)
    @JvmStatic external fun nativeGetSelectedHotbarSlot(): Int
    @JvmStatic external fun nativeSetInventoryOpen(open: Boolean)
    @JvmStatic external fun nativeUpdateSettings(fov: Float, sensitivity: Float, invertY: Boolean, renderDist: Int, graphicsQuality: Int, shadows: Boolean, clouds: Boolean, fog: Boolean, brightness: Float, viewBobbing: Boolean)
        
    @JvmStatic external fun nativeInitSave(dataDir: String)
    @JvmStatic external fun nativeInit(assetManager: AssetManager, dataDir: String, cacheDir: String)

    @JvmStatic external fun nativeGetHealth(): Float
    @JvmStatic external fun nativeGetMaxHealth(): Float
    @JvmStatic external fun nativeIsDead(): Boolean
    @JvmStatic external fun nativeGetHurtTime(): Float
    @JvmStatic external fun nativeRespawn()

    @JvmStatic external fun nativeHasSave(): Boolean
    @JvmStatic external fun nativeLoadGame()
    @JvmStatic external fun nativeNewGame()
    @JvmStatic external fun nativeSaveGame()
    @JvmStatic external fun nativeSwapCraftingSlot(invSlot: Int, craftSlot: Int)
    @JvmStatic external fun nativeSwapCraftingToCrafting(craftSlotA: Int, craftSlotB: Int)
    @JvmStatic external fun nativeTakeCraftingOutput()
    @JvmStatic external fun nativeGetCraftingGrid(): IntArray
    @JvmStatic external fun nativeGetCraftingOutput(): IntArray
  }
}

@Composable
fun MainMenu(onContinueClick: () -> Unit, onNewGameClick: () -> Unit, onDebugWorldClick: () -> Unit, onSettingsClick: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF0F172A))
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "FINECRAFT",
                color = Color.White,
                fontSize = 48.sp,
                fontWeight = FontWeight.Black,
                letterSpacing = 8.sp,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            
            Text(
                text = "C++ OPENGL ENGINE",
                color = Color(0xFF38BDF8),
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium,
                letterSpacing = 4.sp,
                modifier = Modifier.padding(bottom = 64.dp)
            )
            
            val hasSave = remember { MainActivity.nativeHasSave() }
            if (hasSave) {
                Button(
                    onClick = onContinueClick,
                    modifier = Modifier
                        .width(280.dp)
                        .height(56.dp)
                        .padding(bottom = 8.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF22C55E)),
                    shape = RoundedCornerShape(8.dp)
                ) {
                    Text("CONTINUE", fontSize = 18.sp, fontWeight = FontWeight.Bold, letterSpacing = 2.sp)
                }
            }
            
            Button(
                onClick = onNewGameClick,
                modifier = Modifier
                    .width(280.dp)
                    .height(56.dp)
                    .padding(bottom = 8.dp),
                colors = ButtonDefaults.buttonColors(containerColor = if (hasSave) Color(0xFF475569) else Color(0xFF22C55E)),
                shape = RoundedCornerShape(8.dp)
            ) {
                Text("NEW WORLD", fontSize = 18.sp, fontWeight = FontWeight.Bold, letterSpacing = 2.sp)
            }
            
            Button(
                onClick = onDebugWorldClick,
                modifier = Modifier
                    .width(280.dp)
                    .height(56.dp)
                    .padding(bottom = 8.dp),
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF8B5CF6)),
                shape = RoundedCornerShape(8.dp)
            ) {
                Text("DEBUG SHOWCASE", fontSize = 18.sp, fontWeight = FontWeight.Bold, letterSpacing = 2.sp)
            }
            
            Button(
                onClick = onSettingsClick,
                modifier = Modifier
                    .width(280.dp)
                    .height(56.dp),
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF334155)),
                shape = RoundedCornerShape(8.dp)
            ) {
                Text("SETTINGS", fontSize = 18.sp, fontWeight = FontWeight.Bold, letterSpacing = 2.sp, color = Color.White)
            }
        }
    }
}

@Composable
fun SettingsMenu(settings: SettingsManager, onBack: () -> Unit) {
    BackHandler { onBack() }
    var fov by remember { mutableFloatStateOf(settings.fov) }
    var sensitivity by remember { mutableFloatStateOf(settings.sensitivity) }
    var renderDist by remember { mutableIntStateOf(settings.renderDist) }
    var invertY by remember { mutableStateOf(settings.invertY) }
    var graphicsQuality by remember { mutableIntStateOf(settings.graphicsQuality) }
    var shadows by remember { mutableStateOf(settings.shadows) }
    var clouds by remember { mutableStateOf(settings.clouds) }
    var fog by remember { mutableStateOf(settings.fog) }
    var brightness by remember { mutableFloatStateOf(settings.brightness) }
    var viewBobbing by remember { mutableStateOf(settings.viewBobbing) }
    var uiScale by remember { mutableFloatStateOf(settings.uiScale) }
    var controlOpacity by remember { mutableFloatStateOf(settings.controlOpacity) }
    var crosshairSize by remember { mutableFloatStateOf(settings.crosshairSize) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF0F172A))
    ) {
        Column(
            modifier = Modifier.fillMaxSize().padding(32.dp).verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.Top,
            horizontalAlignment = Alignment.Start
        ) {
            Text(
                text = "SETTINGS",
                color = Color.White,
                fontSize = 32.sp,
                fontWeight = FontWeight.Black,
                modifier = Modifier.padding(bottom = 32.dp)
            )
            
            Text("Graphics Quality: ${if(graphicsQuality==0) "Low" else if(graphicsQuality==1) "Medium" else "High"}", color = Color.White)
            Slider(value = graphicsQuality.toFloat(), onValueChange = { graphicsQuality = it.roundToInt(); settings.graphicsQuality = graphicsQuality }, valueRange = 0f..2f, steps = 1)
            
            Text("Render Distance: $renderDist", color = Color.White)
            Slider(value = renderDist.toFloat(), onValueChange = { renderDist = it.roundToInt(); settings.renderDist = renderDist }, valueRange = 2f..10f, steps = 7)
            
            Text("Field of View: ${fov.roundToInt()}", color = Color.White)
            Slider(value = fov, onValueChange = { fov = it; settings.fov = fov }, valueRange = 60f..100f)
            
            Text("Sensitivity: %.2f".format(sensitivity), color = Color.White)
            Slider(value = sensitivity, onValueChange = { sensitivity = it; settings.sensitivity = sensitivity }, valueRange = 0.1f..3.0f)

            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = invertY, onCheckedChange = { invertY = it; settings.invertY = it })
                Text("Invert Y", color = Color.White)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = shadows, onCheckedChange = { shadows = it; settings.shadows = it })
                Text("Shadows (AO)", color = Color.White)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = clouds, onCheckedChange = { clouds = it; settings.clouds = it })
                Text("Clouds", color = Color.White)
            }

            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = fog, onCheckedChange = { fog = it; settings.fog = it })
                Text("Fog", color = Color.White)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = viewBobbing, onCheckedChange = { viewBobbing = it; settings.viewBobbing = it })
                Text("View Bobbing", color = Color.White)
            }


            Text("Brightness: %.1f".format(brightness), color = Color.White)
            Slider(value = brightness, onValueChange = { brightness = it; settings.brightness = it }, valueRange = 0.1f..3.0f)

            Text("UI Scale: %.1f".format(uiScale), color = Color.White)
            Slider(value = uiScale, onValueChange = { uiScale = it; settings.uiScale = it }, valueRange = 0.5f..2.0f)

            Text("Control Opacity: %.1f".format(controlOpacity), color = Color.White)
            Slider(value = controlOpacity, onValueChange = { controlOpacity = it; settings.controlOpacity = it }, valueRange = 0.1f..1.0f)

            Text("Crosshair Size: %.1f".format(crosshairSize), color = Color.White)
            Slider(value = crosshairSize, onValueChange = { crosshairSize = it; settings.crosshairSize = it }, valueRange = 0.5f..2.0f)

            Spacer(modifier = Modifier.height(32.dp))
            Button(
                onClick = onBack,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF334155))
            ) {
                Text("BACK", color = Color.White)
            }
        }
    }
}

@Composable
fun GameplayScreen(settings: SettingsManager, onBack: () -> Unit) {
    var debugMode by remember { mutableStateOf(false) }
    var showInventory by remember { mutableStateOf(false) }
    var atlasBitmap by remember { mutableStateOf<ImageBitmap?>(MainActivity.loadedAtlasBitmap) }
    
    androidx.compose.runtime.LaunchedEffect(Unit) {
        if (atlasBitmap == null) {
            val pixels = MainActivity.nativeGetAtlasPixels()
            if (pixels != null) {
                val w = MainActivity.nativeGetAtlasWidth()
                val h = MainActivity.nativeGetAtlasHeight()
                val bmp = android.graphics.Bitmap.createBitmap(pixels, w, h, android.graphics.Bitmap.Config.ARGB_8888)
                atlasBitmap = bmp.asImageBitmap()
            }
        }
    }
    
    BackHandler {
        if (showInventory) {
            showInventory = false
            MainActivity.nativeSetInventoryOpen(false)
        } else {
            onBack()
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView(
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            MainActivity.nativeSurfaceCreated(holder.surface)
                        }
                        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                            MainActivity.nativeSurfaceChanged(width, height)
                        }
                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            MainActivity.nativeSurfaceDestroyed()
                        }
                    })
                }
            },
            modifier = Modifier.fillMaxSize()
        )

        Box(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        if (!showInventory) {
                            MainActivity.nativeCameraLook(dragAmount.x, dragAmount.y)
                        }
                    }
                }
        )

        if (!showInventory) {
            HUD(
                settings = settings,
                onPause = onBack,
                debugMode = debugMode,
                onToggleDebug = { debugMode = !debugMode; MainActivity.nativeAction("debug_fly") },
                onToggleInventory = { showInventory = true; MainActivity.nativeSetInventoryOpen(true) },
                atlasBitmap = atlasBitmap
            )
        } else {
            InventoryScreen(
                onClose = { showInventory = false; MainActivity.nativeSetInventoryOpen(false) },
                atlasBitmap = atlasBitmap
            )
        }
    }
}

@Composable
fun InventoryScreen(onClose: () -> Unit, atlasBitmap: ImageBitmap?) {
    var inventoryState by remember { mutableStateOf<IntArray>(MainActivity.nativeGetInventory()) }
    var craftingGridState by remember { mutableStateOf<IntArray>(MainActivity.nativeGetCraftingGrid()) }
    var craftingOutputState by remember { mutableStateOf<IntArray>(MainActivity.nativeGetCraftingOutput()) }
    var selectedSlotIndex by remember { mutableStateOf<Int?>(null) }
    var selectedCraftingSlot by remember { mutableStateOf<Int?>(null) }
    
    var splitSourceSlot by remember { mutableStateOf<Int?>(null) }
    var splitSourceType by remember { mutableStateOf<Int>(0) } // 0=inv, 1=craft
    var showSplitDialog by remember { mutableStateOf(false) }
    var splitAmount by remember { mutableIntStateOf(1) }
    var splitDestSlot by remember { mutableStateOf<Int?>(null) }
    var splitDestType by remember { mutableStateOf<Int>(0) }

    LaunchedEffect(Unit) {
        while(true) {
            kotlinx.coroutines.delay(100)
            inventoryState = MainActivity.nativeGetInventory()
            craftingGridState = MainActivity.nativeGetCraftingGrid()
            craftingOutputState = MainActivity.nativeGetCraftingOutput()
        }
    }
    
    if (showSplitDialog && splitSourceSlot != null && splitDestSlot != null) {
        val srcSlot = splitSourceSlot
        val destSlot = splitDestSlot
        if (srcSlot != null && destSlot != null) {
            val srcCount = if (splitSourceType == 0) {
                if (srcSlot * 2 + 1 < inventoryState.size) inventoryState[srcSlot * 2 + 1] else 0
            } else {
                if (srcSlot * 2 + 1 < craftingGridState.size) craftingGridState[srcSlot * 2 + 1] else 0
            }
            if (srcCount > 0) {
                AlertDialog(
                    onDismissRequest = { showSplitDialog = false },
                    title = { Text("Split Stack", color = Color.White) },
                    text = {
                        Column {
                            Text("Select amount to move:", color = Color.White)
                            if (srcCount > 1) {
                                Slider(
                                    value = splitAmount.toFloat(),
                                    onValueChange = { splitAmount = it.toInt() },
                                    valueRange = 1f..srcCount.toFloat(),
                                    steps = if (srcCount > 2) srcCount - 2 else 0
                                )
                            }
                            Text("$splitAmount / $srcCount", color = Color.White)
                        }
                    },
                    confirmButton = {
                        Button(onClick = {
                            showSplitDialog = false
                            MainActivity.nativeMoveItems(splitSourceType, srcSlot, destType = splitDestType, destSlot = destSlot, amount = splitAmount)
                            inventoryState = MainActivity.nativeGetInventory()
                            craftingGridState = MainActivity.nativeGetCraftingGrid()
                            craftingOutputState = MainActivity.nativeGetCraftingOutput()
                            selectedSlotIndex = null
                            selectedCraftingSlot = null
                        }) { Text("Move") }
                    },
                    dismissButton = {
                        Button(onClick = { showSplitDialog = false }) { Text("Cancel") }
                    },
                    containerColor = Color.DarkGray
                )
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.7f)).pointerInput(Unit) {}) {
        Column(
            modifier = Modifier.align(Alignment.Center).background(Color(0xFF222222), RoundedCornerShape(8.dp)).padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("CRAFTING", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(bottom = 8.dp))
            
            // CRAFTING SECTION
            Row(modifier = Modifier.padding(bottom = 16.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    for (row in 0 until 2) {
                        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            for (col in 0 until 2) {
                                val craftSlot = row * 2 + col
                                val cId = craftingGridState[craftSlot * 2]
                                val cCount = craftingGridState[craftSlot * 2 + 1]
                                InventorySlotUI(
                                    slotIndex = craftSlot, 
                                    itemId = cId,
                                    count = cCount,
                                    isSelected = selectedCraftingSlot == craftSlot,
                                    atlasBitmap = atlasBitmap,
                                    onClick = {
                                        if (selectedSlotIndex == null && selectedCraftingSlot == null) {
                                            selectedCraftingSlot = craftSlot
                                        } else if (selectedCraftingSlot != null) {
                                            MainActivity.nativeSwapCraftingToCrafting(selectedCraftingSlot!!, craftSlot)
                                            selectedCraftingSlot = null
                                        } else if (selectedSlotIndex != null) {
                                            MainActivity.nativeSwapCraftingSlot(selectedSlotIndex!!, craftSlot)
                                            selectedSlotIndex = null
                                        }
                                        inventoryState = MainActivity.nativeGetInventory()
                                        craftingGridState = MainActivity.nativeGetCraftingGrid()
                                        craftingOutputState = MainActivity.nativeGetCraftingOutput()
                                    },
                                    onLongClick = {
                                        if (selectedSlotIndex != null) {
                                            splitSourceSlot = selectedSlotIndex
                                            splitSourceType = 0
                                            splitDestSlot = craftSlot
                                            splitDestType = 1
                                            splitAmount = 1
                                            showSplitDialog = true
                                        } else if (selectedCraftingSlot != null && selectedCraftingSlot != craftSlot) {
                                            splitSourceSlot = selectedCraftingSlot
                                            splitSourceType = 1
                                            splitDestSlot = craftSlot
                                            splitDestType = 1
                                            splitAmount = 1
                                            showSplitDialog = true
                                        }
                                    }
                                )
                            }
                        }
                    }
                }
                
                Icon(Icons.AutoMirrored.Filled.ArrowForward, contentDescription = "Craft", tint = Color.White, modifier = Modifier.padding(horizontal = 16.dp))
                
                // Output Slot
                val outId = craftingOutputState[0]
                val outCount = craftingOutputState[1]
                InventorySlotUI(
                    slotIndex = 0,
                    itemId = outId,
                    count = outCount,
                    isSelected = false,
                    atlasBitmap = atlasBitmap,
                    onClick = {
                        MainActivity.nativeTakeCraftingOutput()
                        inventoryState = MainActivity.nativeGetInventory()
                        craftingGridState = MainActivity.nativeGetCraftingGrid()
                        craftingOutputState = MainActivity.nativeGetCraftingOutput()
                    },
                    onLongClick = {}
                )
            }
            Text("INVENTORY", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(bottom = 8.dp))
            
            // Main Inventory (27 slots: 3 rows of 9)
            for (row in 0 until 3) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp), modifier = Modifier.padding(bottom = 4.dp)) {
                    for (col in 0 until 9) {
                        val slotIndex = row * 9 + col
                        val iId = inventoryState[slotIndex * 2]
                        val iCount = inventoryState[slotIndex * 2 + 1]
                        InventorySlotUI(
                            slotIndex = slotIndex, 
                            itemId = iId,
                            count = iCount,
                            isSelected = selectedSlotIndex == slotIndex,
                            atlasBitmap = atlasBitmap,
                            onClick = {
                                if (selectedSlotIndex == null && selectedCraftingSlot == null) {
                                    selectedSlotIndex = slotIndex
                                } else if (selectedSlotIndex != null) {
                                    MainActivity.nativeSwapSlots(selectedSlotIndex!!, slotIndex)
                                    selectedSlotIndex = null
                                    inventoryState = MainActivity.nativeGetInventory()
                                } else if (selectedCraftingSlot != null) {
                                    MainActivity.nativeSwapCraftingSlot(slotIndex, selectedCraftingSlot!!)
                                    selectedCraftingSlot = null
                                    inventoryState = MainActivity.nativeGetInventory()
                                    craftingGridState = MainActivity.nativeGetCraftingGrid()
                                    craftingOutputState = MainActivity.nativeGetCraftingOutput()
                                }
                            },
                            onLongClick = {
                                if (selectedSlotIndex != null && selectedSlotIndex != slotIndex) {
                                    splitSourceSlot = selectedSlotIndex
                                    splitSourceType = 0
                                    splitDestSlot = slotIndex
                                    splitDestType = 0
                                    splitAmount = 1
                                    showSplitDialog = true
                                } else if (selectedCraftingSlot != null) {
                                    splitSourceSlot = selectedCraftingSlot
                                    splitSourceType = 1
                                    splitDestSlot = slotIndex
                                    splitDestType = 0
                                    splitAmount = 1
                                    showSplitDialog = true
                                }
                            }
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
            // Hotbar (9 slots)
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (col in 0 until 9) {
                    val slotIndex = 27 + col
                    val iId = inventoryState[slotIndex * 2]
                    val iCount = inventoryState[slotIndex * 2 + 1]
                    InventorySlotUI(
                        slotIndex = slotIndex, 
                        itemId = iId,
                        count = iCount,
                        isSelected = selectedSlotIndex == slotIndex,
                        atlasBitmap = atlasBitmap,
                        onClick = {
                            if (selectedSlotIndex == null && selectedCraftingSlot == null) {
                                selectedSlotIndex = slotIndex
                            } else if (selectedSlotIndex != null) {
                                MainActivity.nativeSwapSlots(selectedSlotIndex!!, slotIndex)
                                selectedSlotIndex = null
                                inventoryState = MainActivity.nativeGetInventory()
                            } else if (selectedCraftingSlot != null) {
                                MainActivity.nativeSwapCraftingSlot(slotIndex, selectedCraftingSlot!!)
                                selectedCraftingSlot = null
                                inventoryState = MainActivity.nativeGetInventory()
                                craftingGridState = MainActivity.nativeGetCraftingGrid()
                                craftingOutputState = MainActivity.nativeGetCraftingOutput()
                            }
                        },
                        onLongClick = {
                            if (selectedSlotIndex != null && selectedSlotIndex != slotIndex) {
                                splitSourceSlot = selectedSlotIndex
                                splitSourceType = 0
                                splitDestSlot = slotIndex
                                splitDestType = 0
                                splitAmount = 1
                                showSplitDialog = true
                            } else if (selectedCraftingSlot != null) {
                                splitSourceSlot = selectedCraftingSlot
                                splitSourceType = 1
                                splitDestSlot = slotIndex
                                splitDestType = 0
                                splitAmount = 1
                                showSplitDialog = true
                            }
                        }
                    )
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
            Button(onClick = onClose, colors = ButtonDefaults.buttonColors(containerColor = Color.DarkGray)) {
                Text("CLOSE", color = Color.White)
            }
        }
    }
}
@Composable
fun InventorySlotUI(
    slotIndex: Int, 
    itemId: Int,
    count: Int,
    isSelected: Boolean, 
    atlasBitmap: ImageBitmap?, 
    onClick: () -> Unit, 
    onLongClick: () -> Unit = {}
) {
    Box(
        modifier = Modifier
            .size(40.dp)
            .background(if (isSelected) Color.White.copy(alpha = 0.3f) else Color.Black.copy(alpha = 0.5f), RoundedCornerShape(4.dp))
            .border(2.dp, if (isSelected) Color.Yellow else Color.Gray, RoundedCornerShape(4.dp))
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = { onClick() },
                    onLongPress = { onLongClick() }
                )
            }
    ) {
        if (itemId > 0) {
            if (atlasBitmap != null) {
                val isExt = MainActivity.nativeIsUsingExternalAtlas()
                val texIndex = MainActivity.nativeGetItemSprite(itemId)
                val columns = if (isExt) 64 else 16
                val tileSize = if (isExt) 64 else 16
                val tileX = texIndex % columns
                val tileY = texIndex / columns
                val painter = BitmapPainter(
                    image = atlasBitmap,
                    srcOffset = IntOffset(tileX * tileSize, tileY * tileSize),
                    srcSize = IntSize(tileSize, tileSize),
                    filterQuality = FilterQuality.None
                )
                val isTintedGrass = (itemId == 1 || itemId == 5)
                Image(
                    painter = painter,
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize().padding(4.dp),
                    contentScale = ContentScale.Fit,
                    colorFilter = if (isTintedGrass) androidx.compose.ui.graphics.ColorFilter.tint(Color(0xFF70B054), androidx.compose.ui.graphics.BlendMode.Modulate) else null
                )
            } else {
                val color = when(itemId) {
                    1 -> Color(0xFF2D912D)
                    2 -> Color(0xFF785032)
                    3 -> Color(0xFF828282)
                    4 -> Color(0xFF6E4628)
                    5 -> Color(0xFF1E641E)
                    7 -> Color(0xFFA07850)
                    8 -> Color(0xFFE6D296)
                    9 -> Color(0xFF5C3A21)
                    else -> Color.Gray
                }
                Box(modifier = Modifier.fillMaxSize().padding(4.dp).background(color))
            }
            Text(text = "$count", color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.align(Alignment.BottomEnd).padding(2.dp))
        }
    }
}

@Composable
fun HUD(settings: SettingsManager, onPause: () -> Unit, debugMode: Boolean, onToggleDebug: () -> Unit, onToggleInventory: () -> Unit, atlasBitmap: ImageBitmap?) {
    Box(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        // Top Bar
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.Top
        ) {
            IconButton(
                onClick = onPause,
                modifier = Modifier
                    .size(48.dp)
                    .background(Color.Black.copy(alpha = 0.3f), CircleShape)
            ) {
                Icon(Icons.Filled.Close, contentDescription = "Pause", tint = Color.White)
            }
            
            if (debugMode) {
                Text("FPS: 60 | CHUNKS: 12", color = Color.White, fontSize = 12.sp, modifier = Modifier.background(Color.Black.copy(alpha=0.5f)).padding(4.dp))
            }
            
            IconButton(
                onClick = onToggleDebug,
                modifier = Modifier
                    .size(48.dp)
                    .background(Color.Black.copy(alpha = 0.3f), CircleShape)
            ) {
                Icon(Icons.Filled.Settings, contentDescription = "Debug", tint = if (debugMode) Color.Green else Color.White)
            }
            
            Spacer(modifier = Modifier.weight(1f))
            IconButton(
                onClick = onToggleInventory,
                modifier = Modifier
                    .size(48.dp)
                    .background(Color.Black.copy(alpha = 0.3f), CircleShape)
            ) {
                Icon(Icons.Filled.Menu, contentDescription = "Inventory", tint = Color.White)
            }
        }

        // Crosshair
        Box(modifier = Modifier.align(Alignment.Center).size(16.dp)) {
            Box(modifier = Modifier.fillMaxHeight().width(2.dp).background(Color.White.copy(alpha = 0.8f)).align(Alignment.Center))
            Box(modifier = Modifier.fillMaxWidth().height(2.dp).background(Color.White.copy(alpha = 0.8f)).align(Alignment.Center))
        }

        // Joystick
        Joystick(
            modifier = Modifier
                .align(Alignment.BottomStart)
                .padding(bottom = 32.dp, start = 16.dp),
            onMove = { x, y ->
                MainActivity.nativeMoveJoystick(x, y)
            }
        )

        
        // Sprint
        ActionButton(
            icon = Icons.AutoMirrored.Filled.ArrowForward,
            modifier = Modifier.align(Alignment.BottomStart).padding(bottom = 160.dp, start = 16.dp),
            onDown = { MainActivity.nativeAction("sprint_down") },
            onUp = { MainActivity.nativeAction("sprint_up") }
        )

        // Action Buttons
        Box(
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(bottom = 32.dp, end = 16.dp)
                .size(160.dp)
        ) {
            // Jump
            ActionButton(
                icon = Icons.AutoMirrored.Filled.ArrowForward,
                modifier = Modifier.align(Alignment.TopCenter).rotate(-90f),
                onDown = { MainActivity.nativeAction("jump_down") },
                onUp = { MainActivity.nativeAction("jump_up") }
            )
            // Crouch
            ActionButton(
                icon = Icons.AutoMirrored.Filled.ArrowForward,
                modifier = Modifier.align(Alignment.BottomCenter).rotate(90f),
                onDown = { MainActivity.nativeAction("sneak") }
            )
            // Place
            ActionButton(
                icon = Icons.Filled.Add,
                modifier = Modifier.align(Alignment.CenterStart),
                onDown = { MainActivity.nativeAction("place_down") },
                onUp = { MainActivity.nativeAction("place_up") }
            )
            // Break
            ActionButton(
                icon = Icons.Filled.Close,
                modifier = Modifier.align(Alignment.CenterEnd),
                onDown = { MainActivity.nativeAction("break_down") },
                onUp = { MainActivity.nativeAction("break_up") }
            )
        }

        // Health
        var health by remember { mutableFloatStateOf(MainActivity.nativeGetHealth()) }
        var maxHealth by remember { mutableFloatStateOf(MainActivity.nativeGetMaxHealth()) }
        var hurtTime by remember { mutableFloatStateOf(MainActivity.nativeGetHurtTime()) }
        var isDead by remember { mutableStateOf(MainActivity.nativeIsDead()) }
        
        LaunchedEffect(Unit) {
            while(true) {
                kotlinx.coroutines.delay(50)
                health = MainActivity.nativeGetHealth()
                maxHealth = MainActivity.nativeGetMaxHealth()
                hurtTime = MainActivity.nativeGetHurtTime()
                isDead = MainActivity.nativeIsDead()
            }
        }
        
        if (hurtTime > 0f) {
            Box(modifier = Modifier.fillMaxSize().background(Color.Red.copy(alpha = (hurtTime / 0.5f) * 0.4f)))
        }
        
        Row(
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 70.dp),
            horizontalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            val fullHearts = (health / 2).toInt()
            val halfHeart = (health % 2) >= 1f
            val maxHearts = (maxHealth / 2).toInt()
            
            val isExt = MainActivity.nativeIsUsingExternalAtlas()
            
            for (i in 0 until maxHearts) {
                Box(modifier = Modifier.size(24.dp)) {
                    // Always use fallback vector icons for hearts as Faithful atlas doesn't include GUI elements
                    Icon(androidx.compose.material.icons.Icons.Filled.FavoriteBorder, contentDescription = null, tint = androidx.compose.ui.graphics.Color.Black.copy(alpha = 0.5f), modifier = Modifier.fillMaxSize())
                    if (i < fullHearts) {
                        Icon(androidx.compose.material.icons.Icons.Filled.Favorite, contentDescription = null, tint = androidx.compose.ui.graphics.Color.Red, modifier = Modifier.fillMaxSize())
                    } else if (i == fullHearts && halfHeart) {
                        Box(modifier = Modifier.fillMaxHeight().fillMaxWidth(0.5f).clipToBounds()) {
                            Icon(androidx.compose.material.icons.Icons.Filled.Favorite, contentDescription = null, tint = androidx.compose.ui.graphics.Color.Red, modifier = Modifier.size(24.dp))
                        }
                    }
                }
            }
        }
        
        if (isDead) {
            Box(modifier = Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.8f)).clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}) {
                Column(
                    modifier = Modifier.align(Alignment.Center),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(24.dp)
                ) {
                    Text("YOU DIED", color = Color.Red, fontSize = 48.sp, fontWeight = FontWeight.Bold)
                    
                    Button(
                        onClick = { MainActivity.nativeRespawn() },
                        colors = ButtonDefaults.buttonColors(containerColor = Color.DarkGray)
                    ) {
                        Text("RESPAWN", color = Color.White, fontSize = 20.sp)
                    }
                    
                    Button(
                        onClick = { onPause() },
                        colors = ButtonDefaults.buttonColors(containerColor = Color.DarkGray)
                    ) {
                        Text("MAIN MENU", color = Color.White, fontSize = 20.sp)
                    }
                }
            }
        }

        // Hotbar
        var inventoryState by remember { mutableStateOf<IntArray>(MainActivity.nativeGetInventory()) }
        var selectedSlot by remember { mutableIntStateOf(MainActivity.nativeGetSelectedHotbarSlot()) }
        
        // Polling inventory state
        LaunchedEffect(Unit) {
            while(true) {
                kotlinx.coroutines.delay(100)
                inventoryState = MainActivity.nativeGetInventory()
                selectedSlot = MainActivity.nativeGetSelectedHotbarSlot()
            }
        }

        Row(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            for (i in 0 until 9) {
                val invIndex = 27 + i
                val itemId = inventoryState[invIndex * 2]
                val count = inventoryState[invIndex * 2 + 1]
                
                InventorySlotUI(
                    slotIndex = invIndex,
                    itemId = itemId,
                    count = count,
                    isSelected = (i == selectedSlot),
                    atlasBitmap = atlasBitmap,
                    onClick = {
                        MainActivity.nativeAction("select_slot_$i")
                    },
                    onLongClick = {}
                )
            }
        }
    }
}
@Composable
fun ActionButton(icon: androidx.compose.ui.graphics.vector.ImageVector, modifier: Modifier = Modifier, onDown: () -> Unit = {}, onUp: () -> Unit = {}) {
    Box(
        modifier = modifier
            .size(56.dp)
            .clip(CircleShape)
            .background(Color.Black.copy(alpha = 0.3f))
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        onDown()
                        tryAwaitRelease()
                        onUp()
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Icon(icon, contentDescription = null, tint = Color.White.copy(alpha = 0.8f), modifier = Modifier.size(28.dp))
    }
}

@Composable
fun Joystick(modifier: Modifier = Modifier, onMove: (Float, Float) -> Unit) {
    var offsetX by remember { mutableFloatStateOf(0f) }
    var offsetY by remember { mutableFloatStateOf(0f) }
    val maxRadius = 100f
    Box(
        modifier = modifier
            .size(120.dp)
            .clip(CircleShape)
            .background(Color.Black.copy(alpha = 0.2f))
            .border(1.dp, Color.White.copy(alpha = 0.2f), CircleShape)
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragEnd = {
                        offsetX = 0f
                        offsetY = 0f
                        onMove(0f, 0f)
                    },
                    onDragCancel = {
                        offsetX = 0f
                        offsetY = 0f
                        onMove(0f, 0f)
                    }
                ) { change, dragAmount ->
                    change.consume()
                    val newX = offsetX + dragAmount.x
                    val newY = offsetY + dragAmount.y
                    val distance = sqrt(newX * newX + newY * newY)
                    if (distance <= maxRadius) {
                        offsetX = newX
                        offsetY = newY
                    } else {
                        offsetX = (newX / distance) * maxRadius
                        offsetY = (newY / distance) * maxRadius
                    }
                    onMove(offsetX / maxRadius, offsetY / maxRadius)
                }
            },
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .offset { IntOffset(offsetX.roundToInt(), offsetY.roundToInt()) }
                .size(48.dp)
                .clip(CircleShape)
            .background(Color.White.copy(alpha = 0.6f))
        )
    }
}
