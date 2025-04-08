// sharedState.js

export const sharedState = {
    activeManager: null,
    setActiveManager(managerType) {
        this.activeManager = managerType;
    },
    clearActiveManager() {
        this.activeManager = null;
    },
    isManagerActive(managerType) {
        return this.activeManager === managerType;
    },
    checkMouseState(mouse) {
        // Clear active manager when mouse button is released
        if (!mouse.button && this.activeManager) {
            this.clearActiveManager();
        }
    }
};