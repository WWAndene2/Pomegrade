package me.magnum.melonds.ui.cheats.ui.item

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Folder
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import me.magnum.melonds.domain.model.Game
import me.magnum.melonds.ui.common.MelonPreviewSet
import me.magnum.melonds.ui.theme.MelonTheme
import me.magnum.melonds.ui.theme.PunicaDimens

@Composable
fun GameItem(
    modifier: Modifier,
    game: Game,
    onClick: () -> Unit,
) {
    Row(
        modifier = modifier
            .clickable { onClick() }
            // the settings rows' metrics (preference_punica): 64 dp high at least, 16 dp sides, 12 dp to the text
            .heightIn(min = PunicaDimens.ROW_MIN.dp)
            .padding(horizontal = PunicaDimens.MARGIN.dp),
        horizontalArrangement = Arrangement.spacedBy(PunicaDimens.GAP_WIDE.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            modifier = Modifier.padding(vertical = 12.dp).size(24.dp),
            imageVector = Icons.Filled.Folder,
            contentDescription = null,
            tint = MaterialTheme.colors.secondary,
        )
        Text(
            modifier = Modifier.weight(1f),
            text = game.name,
        )
    }
}

@MelonPreviewSet
@Composable
private fun PreviewGameItem() {
    MelonTheme {
        GameItem(
            modifier = Modifier.fillMaxWidth(),
            game = Game(0, "Super Cool Game", "", "", emptyList()),
            onClick = { },
        )
    }
}