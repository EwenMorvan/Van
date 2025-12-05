package com.van.management.ui.tabs;

import android.app.AlertDialog;
import android.util.Log;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.content.ContextCompat;

import com.van.management.R;
import com.van.management.data.ProjectorCommand;
import com.van.management.data.VanCommand;
import com.van.management.data.VanState;

/**
 * Gère l'onglet Multimedia avec le multimédia
 */
public class MultimediaTab extends BaseTab {
    private static final String TAG = "MultimediaTab";

    // VanCinema Components
    private ImageView cinemaIcon;
    private ImageView headerBtIcon;
    private TextView cinemaStatusText;
    private TextView cinemaPositionText;
    private TextView cinemaDisconnectedMessage;
    private ConstraintLayout btnDeployRetract;
    private TextView btnDeployRetractText;
    private ConstraintLayout btnUp1mm;
    private ConstraintLayout btnUp01mm;
    private ConstraintLayout btnDown01mm;
    private ConstraintLayout btnDown1mm;
    private ImageButton btnAdvancedSettings;

    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du MultimediaTab");

        if (rootView == null) return;

        // Initialize VanCinema views
        headerBtIcon = rootView.findViewById(R.id.header_bt_icon_bg);
        cinemaIcon = rootView.findViewById(R.id.cinema_icon);
        cinemaStatusText = rootView.findViewById(R.id.cinema_status_text);
        cinemaPositionText = rootView.findViewById(R.id.cinema_position_text);
        cinemaDisconnectedMessage = rootView.findViewById(R.id.cinema_disconnected_message);
        btnDeployRetract = rootView.findViewById(R.id.btn_deploy_retract);
        btnDeployRetractText = rootView.findViewById(R.id.btn_deploy_retract_text);
        btnUp1mm = rootView.findViewById(R.id.btn_up_1mm);
        btnUp01mm = rootView.findViewById(R.id.btn_up_01mm);
        btnDown01mm = rootView.findViewById(R.id.btn_down_01mm);
        btnDown1mm = rootView.findViewById(R.id.btn_down_1mm);
        btnAdvancedSettings = rootView.findViewById(R.id.btn_advanced_settings);

        // Setup button listeners
        if (btnDeployRetract != null) {
            btnDeployRetract.setOnClickListener(v -> {
                String buttonText = btnDeployRetractText.getText().toString();
                if (buttonText.equals("DEPLOY")) {
                    sendProjectorCommand(ProjectorCommand.deploy());
                } else if (buttonText.equals("RETRACT")) {
                    sendProjectorCommand(ProjectorCommand.retract());
                } else if (buttonText.equals("STOP")) {
                    sendProjectorCommand(ProjectorCommand.stop());
                }
            });
        }
        if (btnUp1mm != null) {
            btnUp1mm.setOnClickListener(v -> sendProjectorCommand(ProjectorCommand.jogUp1()));
        }
        if (btnUp01mm != null) {
            btnUp01mm.setOnClickListener(v -> sendProjectorCommand(ProjectorCommand.jogUp01()));
        }
        if (btnDown01mm != null) {
            btnDown01mm.setOnClickListener(v -> sendProjectorCommand(ProjectorCommand.jogDown01()));
        }
        if (btnDown1mm != null) {
            btnDown1mm.setOnClickListener(v -> sendProjectorCommand(ProjectorCommand.jogDown1()));
        }

        // Setup advanced settings button
        if (btnAdvancedSettings != null) {
            btnAdvancedSettings.setOnClickListener(v -> showAdvancedSettingsDialog());
        }
    }

    @Override
    public void updateUI(VanState vanState) {
        if (rootView == null || vanState == null) return;
        Log.d(TAG, "Mise à jour UI avec VanState");

        if (vanState.projector != null) {
            updateProjectorUI(vanState.projector);
        }
    }

    /**
     * Envoie une commande du projecteur via VanCommand
     */
    private void sendProjectorCommand(ProjectorCommand projectorCmd) {
        VanCommand command = VanCommand.createProjectorCommand(projectorCmd);
        Log.d(TAG, "Envoi commande: " + projectorCmd.toString());
        Log.d(TAG, command.toHexString());

        // Envoyer la commande via le commandSender fourni par MainActivity
        if (commandSender != null) {
            boolean result = commandSender.sendBinaryCommand(command);
            if (result) {
                Log.d(TAG, "✅ Commande envoyée avec succès");
            } else {
                Log.e(TAG, "❌ Échec d'envoi de la commande");
            }
        } else {
            Log.e(TAG, "⚠️ commandSender is null - vérifiez que MainActivity l'a défini");
        }
    }

    /**
     * Met à jour l'UI selon l'état du projecteur
     */
    private void updateProjectorUI(VanState.ProjectorData projector) {
        // Update BLE connection status in header
        if (headerBtIcon != null) {
            int btColor = projector.connected ? 
                    ContextCompat.getColor(rootView.getContext(), R.color.connected) :
                    ContextCompat.getColor(rootView.getContext(), R.color.error);
            headerBtIcon.setColorFilter(btColor);

        }

        // Update projector status with state-based colors (only if connected)
        if (cinemaStatusText != null && projector.connected) {
            String statusText;
            int statusColor;

            switch (projector.state) {
                case PROJECTOR_STATE_DEPLOYED:
                    statusText = "DEPLOYED";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.connected);
                    break;
                case PROJECTOR_STATE_DEPLOYING:
                    statusText = "DEPLOYING...";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.connected);
                    break;
                case PROJECTOR_STATE_RETRACTED:
                    statusText = "RETRACTED";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.gray);
                    break;
                case PROJECTOR_STATE_RETRACTING:
                    statusText = "RETRACTING...";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.gray);
                    break;
                case PROJECTOR_STATE_STOPPED:
                    statusText = "STOPPED";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.error);
                    break;
                default:
                    statusText = "UNKNOWN";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.error);
            }

            cinemaStatusText.setText(statusText);
            cinemaStatusText.setTextColor(statusColor);
        }

        // Update projector icon color based on state
        if (cinemaIcon != null) {
            int iconColor;
            if (projector.connected) {
                switch (projector.state) {
                    case PROJECTOR_STATE_DEPLOYED:
                    case PROJECTOR_STATE_DEPLOYING:
                        iconColor = ContextCompat.getColor(rootView.getContext(), R.color.connected);
                        break;
                    case PROJECTOR_STATE_RETRACTED:
                    case PROJECTOR_STATE_RETRACTING:
                        iconColor = ContextCompat.getColor(rootView.getContext(), R.color.gray);
                        break;
                    case PROJECTOR_STATE_STOPPED:
                    default:
                        iconColor = ContextCompat.getColor(rootView.getContext(), R.color.error);
                }
            } else {
                iconColor = ContextCompat.getColor(rootView.getContext(), R.color.error);
            }
            cinemaIcon.setColorFilter(iconColor);
        }

        // Update Deploy/Retract/Stop button text based on state
        if (btnDeployRetractText != null && projector.connected) {
            switch (projector.state) {
                case PROJECTOR_STATE_DEPLOYED:
                    btnDeployRetractText.setText("RETRACT");
                    break;
                case PROJECTOR_STATE_RETRACTED:
                    btnDeployRetractText.setText("DEPLOY");
                    break;
                case PROJECTOR_STATE_STOPPED:
                    // If stopped, check position to determine next action
                    if (projector.position_percent > 50.0f) {
                        btnDeployRetractText.setText("DEPLOY");
                    } else {
                        btnDeployRetractText.setText("RETRACT");
                    }
                    break;
                case PROJECTOR_STATE_DEPLOYING:
                case PROJECTOR_STATE_RETRACTING:
                    btnDeployRetractText.setText("STOP");
                    break;
                default:
                    btnDeployRetractText.setText("DEPLOY");
            }
        }

        // Update position text visibility and content
        if (cinemaPositionText != null && cinemaDisconnectedMessage != null) {
            if (projector.connected) {
                cinemaPositionText.setText(String.format("Position: %.1f%%", projector.position_percent));
                cinemaPositionText.setVisibility(View.VISIBLE);
                cinemaDisconnectedMessage.setVisibility(View.GONE);
            } else {
                cinemaPositionText.setVisibility(View.INVISIBLE);
                cinemaDisconnectedMessage.setVisibility(View.VISIBLE);
                cinemaIcon.setColorFilter(ContextCompat.getColor(rootView.getContext(), R.color.gray));
                cinemaStatusText.setTextColor(ContextCompat.getColor(rootView.getContext(), R.color.gray));
                cinemaStatusText.setText("UNKNOWN");
            }
        }

        // Disable/Enable control buttons based on connection
        setButtonsEnabled(projector.connected);

        Log.d(TAG, "UI mise à jour - État: " + projector.state + ", Connecté: " + projector.connected);
    }

    /**
     * Active ou désactive les boutons de contrôle
     */
    private void setButtonsEnabled(boolean enabled) {
        if (btnDeployRetract != null) {
            btnDeployRetract.setEnabled(enabled);
            btnDeployRetract.setAlpha(enabled ? 1.0f : 0.5f);
        }
        if (btnUp1mm != null) {
            btnUp1mm.setEnabled(enabled);
            btnUp1mm.setAlpha(enabled ? 1.0f : 0.5f);
        }
        if (btnUp01mm != null) {
            btnUp01mm.setEnabled(enabled);
            btnUp01mm.setAlpha(enabled ? 1.0f : 0.5f);
        }
        if (btnDown01mm != null) {
            btnDown01mm.setEnabled(enabled);
            btnDown01mm.setAlpha(enabled ? 1.0f : 0.5f);
        }
        if (btnDown1mm != null) {
            btnDown1mm.setEnabled(enabled);
            btnDown1mm.setAlpha(enabled ? 1.0f : 0.5f);
        }
        if (btnAdvancedSettings != null) {
            btnAdvancedSettings.setEnabled(enabled);
            btnAdvancedSettings.setAlpha(enabled ? 1.0f : 0.5f);
        }
    }    /**
     * Affiche le dialog pour les commandes avancées (forced jogs + calibration)
     */
    private void showAdvancedSettingsDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(rootView.getContext());
        builder.setTitle("Advanced Projector Controls")
                .setMessage("⚠️ WARNING: These commands are advanced and may affect projector calibration.\n\n" +
                        "Use with caution!")
                .setPositiveButton("Continue", (dialog, which) -> showAdvancedCommandsMenu())
                .setNegativeButton("Cancel", (dialog, which) -> dialog.dismiss())
                .show();
    }

    /**
     * Affiche le menu des commandes avancées
     */
    private void showAdvancedCommandsMenu() {
        final String[] commands = {
                "🔧 Jog Up 1.0 Turn (Forced)",
                "🔧 Jog Down 1.0 Turn (Forced)",
                "⚙️ Calibrate Up",
                "⚙️ Calibrate Down",
                "",
                "💡 Tip: Use Jog Forced to find min/max, then Calibrate to save limits"
        };

        AlertDialog.Builder builder = new AlertDialog.Builder(rootView.getContext());
        builder.setTitle("Select Advanced Command")
                .setItems(commands, (dialog, which) -> {
                    // Ignore clicks on empty line and tip (indices 4 and 5)
                    if (which >= 4) {
                        return;
                    }
                    
                    switch (which) {
                        case 0:
                            sendProjectorCommand(ProjectorCommand.jogUp1Forced());
                            break;
                        case 1:
                            sendProjectorCommand(ProjectorCommand.jogDown1Forced());
                            break;
                        case 2:
                            sendProjectorCommand(ProjectorCommand.calibrateUp());
                            break;
                        case 3:
                            sendProjectorCommand(ProjectorCommand.calibrateDown());
                            break;
                    }
                })
                .show();
    }
}
