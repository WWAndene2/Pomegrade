package me.magnum.melonds.ui.common.preference

import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.material.ContentAlpha
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import me.magnum.melonds.ui.common.component.text.CaptionText
import me.magnum.melonds.ui.theme.PunicaDimens

@Composable
fun ActionLauncherItem(
    name: String,
    value: String,
    enabled: Boolean = true,
    onLaunchAction: () -> Unit,
    horizontalPadding: Dp = PunicaDimens.MARGIN.dp,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .run {
                if (enabled) {
                    clickable { onLaunchAction() }.focusable()
                } else {
                    this
                }
            }
            .heightIn(min = PunicaDimens.ROW_MIN.dp)
            .padding(start = horizontalPadding, end = horizontalPadding, top = PunicaDimens.GAP_WIDE.dp, bottom = PunicaDimens.GAP_WIDE.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        CompositionLocalProvider(LocalContentAlpha provides if (enabled) ContentAlpha.high else ContentAlpha.disabled) {
            Text(
                text = name,
                style = MaterialTheme.typography.body1,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            CaptionText(
                text = value,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}