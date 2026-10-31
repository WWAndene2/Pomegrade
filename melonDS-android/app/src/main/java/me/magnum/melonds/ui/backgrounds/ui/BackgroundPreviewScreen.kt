package me.magnum.melonds.ui.backgrounds.ui

import android.annotation.SuppressLint
import android.app.Activity
import android.widget.Toast
import androidx.compose.animation.AnimatedContentScope
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.ExperimentalSharedTransitionApi
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.statusBarsIgnoringVisibility
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import coil.compose.AsyncImage
import coil.request.ImageRequest
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.Background
import me.magnum.melonds.ui.common.component.PunicaHeader

@SuppressLint("UnusedMaterialScaffoldPaddingParameter")
@OptIn(ExperimentalLayoutApi::class, ExperimentalSharedTransitionApi::class)
@Composable
fun BackgroundPreviewScreen(
    background: Background,
    sharedTransitionScope: SharedTransitionScope,
    animatedContentScope: AnimatedContentScope,
    onBackClick: () -> Unit,
) {
    var isWindowDecorVisible by remember { mutableStateOf(true) }

    val context = LocalContext.current
    val view = LocalView.current
    val insetsController = remember(context, view) {
        val window = (context as Activity).window
        WindowCompat.getInsetsController(window, view)
    }

    LaunchedEffect(isWindowDecorVisible) {
        if (isWindowDecorVisible) {
            insetsController.show(WindowInsetsCompat.Type.navigationBars())
        } else {
            insetsController.hide(WindowInsetsCompat.Type.navigationBars())
        }
    }

    DisposableEffect(insetsController) {
        insetsController.hide(WindowInsetsCompat.Type.statusBars())
        onDispose {
            insetsController.show(WindowInsetsCompat.Type.systemBars())
        }
    }

    Scaffold(backgroundColor = Color.Black) {
        with(sharedTransitionScope) {
            AsyncImage(
                modifier = Modifier
                    .sharedElement(
                        sharedContentState = sharedTransitionScope.rememberSharedContentState(background.id?.toString().orEmpty()),
                        animatedVisibilityScope = animatedContentScope,
                    )
                    .fillMaxSize()
                    .clickable(interactionSource = null, indication = null) { isWindowDecorVisible = !isWindowDecorVisible },
                model = ImageRequest.Builder(context)
                    .data(background.uri)
                    .listener(
                        onError = { _, _ ->
                            Toast.makeText(context, R.string.layout_background_load_failed, Toast.LENGTH_LONG).show()
                        },
                    )
                    .build(),
                contentDescription = null
            )
        }

        AnimatedVisibility(
            modifier = Modifier.fillMaxWidth().windowInsetsPadding(WindowInsets.statusBarsIgnoringVisibility),
            visible = isWindowDecorVisible,
            enter = slideInVertically(initialOffsetY = { -it }) + fadeIn(),
            exit = slideOutVertically(targetOffsetY = { -it }) + fadeOut(),
        ) {
            AppBar(title = background.name, onBackClick = onBackClick)
        }
    }
}

/** Punica's header over the picture, on a black-to-clear fade so it stays readable on a light background. */
@Composable
private fun AppBar(title: String, onBackClick: () -> Unit) {
    PunicaHeader(
        subtitle = title,
        onBack = onBackClick,
        modifier = Modifier.background(Brush.verticalGradient(listOf(Color.Black, Color.Transparent))),
    )
}
