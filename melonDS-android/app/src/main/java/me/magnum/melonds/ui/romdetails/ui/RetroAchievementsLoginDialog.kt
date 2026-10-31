package me.magnum.melonds.ui.romdetails.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.OutlinedTextField
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.autofill.ContentType
import androidx.compose.ui.autofill.contentType
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import me.magnum.melonds.R
import me.magnum.melonds.ui.common.component.dialog.BaseDialog
import me.magnum.melonds.ui.common.component.dialog.DialogButton
import me.magnum.melonds.ui.common.melonOutlinedTextFieldColors
import me.magnum.melonds.ui.theme.PunicaDimens

/** Punica's dialog (BaseDialog: its card, title and buttons, as every other dialog) with the two login fields. */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun RetroAchievementsLoginDialog(
    existingUsername: String?,
    onDismiss: () -> Unit,
    onLogin: (username: String, password: String) -> Unit,
) {
    var username by rememberSaveable {
        mutableStateOf(existingUsername.orEmpty())
    }
    var password by remember {
        mutableStateOf("")
    }

    BaseDialog(
        title = stringResource(id = R.string.login_with_retro_achievements),
        onDismiss = onDismiss,
        content = { padding ->
            Column(
                modifier = Modifier.padding(padding).padding(vertical = PunicaDimens.GAP.dp),
                verticalArrangement = Arrangement.spacedBy(PunicaDimens.GAP.dp),
            ) {
                OutlinedTextField(
                    modifier = Modifier
                        .fillMaxWidth()
                        .contentType(ContentType.Username),
                    value = username,
                    onValueChange = { username = it },
                    colors = melonOutlinedTextFieldColors(),
                    label = {
                        Text(text = stringResource(id = R.string.username))
                    }
                )

                OutlinedTextField(
                    modifier = Modifier
                        .fillMaxWidth()
                        .contentType(ContentType.Password),
                    value = password,
                    onValueChange = { password = it },
                    visualTransformation = PasswordVisualTransformation(),
                    keyboardOptions = KeyboardOptions(
                        autoCorrectEnabled = false,
                        keyboardType = KeyboardType.Password
                    ),
                    colors = melonOutlinedTextFieldColors(),
                    label = {
                        Text(text = stringResource(id = R.string.password))
                    }
                )
            }
        },
        buttons = {
            DialogButton(text = stringResource(id = R.string.cancel), onClick = onDismiss)
            DialogButton(text = stringResource(id = R.string.login), onClick = { onLogin(username, password) })
        },
    )
}
