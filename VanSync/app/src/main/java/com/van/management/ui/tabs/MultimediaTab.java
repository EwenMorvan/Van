package com.van.management.ui.tabs;

import android.util.Log;
import android.view.View;
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
    private TextView cinemaStatusText;
    private TextView cinemaPositionText;
    private ConstraintLayout btnDeployRetract;
    private TextView btnDeployRetractText;
    private ConstraintLayout btnUp1mm;
    private ConstraintLayout btnUp01mm;
    private ConstraintLayout btnDown01mm;
    private ConstraintLayout btnDown1mm;
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du MultimediaTab");
        
        if (rootView == null) return;
        
        // Initialize VanCinema views
        cinemaIcon = rootView.findViewById(R.id.cinema_icon);
        cinemaStatusText = rootView.findViewById(R.id.cinema_status_text);
        cinemaPositionText = rootView.findViewById(R.id.cinema_position_text);
        btnDeployRetract = rootView.findViewById(R.id.btn_deploy_retract);
        btnDeployRetractText = rootView.findViewById(R.id.btn_deploy_retract_text);
        btnUp1mm = rootView.findViewById(R.id.btn_up_1mm);
        btnUp01mm = rootView.findViewById(R.id.btn_up_01mm);
        btnDown01mm = rootView.findViewById(R.id.btn_down_01mm);
        btnDown1mm = rootView.findViewById(R.id.btn_down_1mm);
        
        // Setup button listeners
        if (btnDeployRetract != null) {
            btnDeployRetract.setOnClickListener(v -> sendProjectorCommand(
                btnDeployRetractText.getText().toString().equals("DEPLOY") ? 
                ProjectorCommand.deploy() : ProjectorCommand.retract()
            ));
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
        
        // Envoyer la commande (la méthode sendCommand doit être disponible dans BaseTab)
        if (this instanceof CommandSender) {
            ((CommandSender) this).sendCommand(command);
        }
    }
    
    /**
     * Met à jour l'UI selon l'état du projecteur
     */
    private void updateProjectorUI(VanState.ProjectorData projector) {
        if (cinemaStatusText != null) {
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
                    statusText = "CLOSED";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.disconnected);
                    break;
                case PROJECTOR_STATE_RETRACTING:
                    statusText = "RETRACTING...";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.disconnected);
                    break;
                default:
                    statusText = "UNKNOWN";
                    statusColor = ContextCompat.getColor(rootView.getContext(), R.color.disconnected);
            }
            
            cinemaStatusText.setText(statusText);
            cinemaStatusText.setTextColor(statusColor);
        }
        
        if (cinemaIcon != null) {
            int iconColor = (projector.state == VanState.ProjectorState.PROJECTOR_STATE_DEPLOYED ||
                           projector.state == VanState.ProjectorState.PROJECTOR_STATE_DEPLOYING) ?
                    ContextCompat.getColor(rootView.getContext(), R.color.connected) :
                    ContextCompat.getColor(rootView.getContext(), R.color.disconnected);
            cinemaIcon.setColorFilter(iconColor);
        }
        
        if (btnDeployRetractText != null) {
            boolean isDeployed = projector.state == VanState.ProjectorState.PROJECTOR_STATE_DEPLOYED ||
                               projector.state == VanState.ProjectorState.PROJECTOR_STATE_DEPLOYING;
            btnDeployRetractText.setText(isDeployed ? "RETRACT" : "DEPLOY");
        }
        
        Log.d(TAG, "UI mise à jour - État: " + projector.state + ", Connecté: " + projector.connected);
    }
    
    /**
     * Interface pour envoyer des commandes (à implémenter dans BaseTab ou Activity)
     */
    public interface CommandSender {
        void sendCommand(VanCommand command);
    }
}
