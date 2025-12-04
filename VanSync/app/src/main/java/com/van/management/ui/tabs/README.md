# Architecture des Tabs

## Structure

Chaque tab de l'application est maintenant géré par une classe dédiée dans le package `com.van.management.ui.tabs`.

### Classes principales

- **BaseTab** : Classe abstraite de base pour tous les tabs
  - `initialize(View)` : Initialise le tab avec sa vue racine
  - `updateUI(VanState)` : Met à jour l'UI avec les nouvelles données
  - `onTabSelected()` : Appelé quand le tab devient visible
  - `onTabDeselected()` : Appelé quand le tab n'est plus visible

### Tabs implémentés

1. **HomeTab** : Onglet d'accueil avec vue d'ensemble
2. **LightTab** : Gestion des lumières (intérieur/extérieur, modes, brightness)
3. **WaterTab** : Gestion de l'eau (à implémenter)
4. **HeaterTab** : Gestion du chauffage (à implémenter)
5. **EnergyTab** : Gestion de l'énergie/batterie (à implémenter)
6. **MultimediaTab** : Gestion multimédia (à implémenter)

## Fonctionnalités de LightTab

### Modes d'éclairage
- **Intérieur** : 6 modes (Off, White, Yellow, Blue, Rainbow, Party)
- **Extérieur** : 4 modes (Off, Warm, Cool, RGB)
- Sélection par touch avec highlight orange
- Effets de cercles colorés avec blur RenderScript

### Brightness Sliders
- Sliders interactifs pour intérieur et extérieur
- Valeur minimale bloquée à 10%
- Mise à jour en temps réel du pourcentage

## Avantages de cette architecture

1. **Séparation des responsabilités** : Chaque tab gère sa propre logique
2. **Maintenabilité** : Code plus facile à maintenir et debugger
3. **Réutilisabilité** : Les tabs peuvent être réutilisés dans d'autres contextes
4. **Testabilité** : Chaque tab peut être testé indépendamment
5. **Performance** : Initialisation lazy des tabs (uniquement au premier affichage)

## Utilisation dans MainActivity

```java
// Initialisation des tabs
private void initializeTabManagers() {
    lightTab = new LightTab(this, renderScript);
    homeTab = new HomeTab();
    // ...
}

// Chargement d'un tab
private void loadTabContent(Tab tab) {
    BaseTab tabManager = tabManagers.get(tab.ordinal());
    if (isFirstLoad && tabManager != null) {
        tabManager.initialize(contentView);
    }
    tabManager.updateUI(lastVanState);
}
```

## TODO

- [ ] Implémenter la logique complète de HomeTab
- [ ] Implémenter WaterTab
- [ ] Implémenter HeaterTab
- [ ] Implémenter EnergyTab
- [ ] Implémenter MultimediaTab
- [ ] Ajouter l'envoi des commandes BLE depuis LightTab
- [ ] Persister les sélections (modes, brightness) avec PreferencesManager
