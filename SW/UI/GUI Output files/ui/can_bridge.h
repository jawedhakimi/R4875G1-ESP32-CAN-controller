#ifndef CAN_BRIDGE_H
#define CAN_BRIDGE_H

/* Polls the PSU over CAN, decodes responses, feeds the energy meter, checks
   CAN bus health, and mirrors live telemetry + output state into the UI.
   When the PSU stops responding (isStale()), shows "--"/"NO LINK" instead
   of continuing to display the last cached numbers forever. Call every
   loop. */
void handle_can_and_ui();

#endif
