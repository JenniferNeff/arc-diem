module.exports = [
  {
    "type": "heading",
    "defaultValue": "Arc Diem Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Daytime hours"
      },
      {
        "type": "slider",
        "messageKey": "DayStart",
        "defaultValue": 7,
        "label": "Day starts at:",
        "min": 0,
        "max": 23
      },
      {
        "type": "slider",
        "messageKey": "DayEnd",
        "defaultValue": 23,
        "label": "Day ends at:",
        "min": 0,
        "max": 23,
        "description": "Choose hours from 0 to 23. You can set the end time \"earlier\" than the start time, if you like."
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Notifications"
      },
      {
        "type": "radiogroup",
        "messageKey": "BatteryStatus",
        "label": "Display battery status?",
        "defaultValue": "low",
        "options": [
          {
            "label":"Yes",
            "value":"yes"
          },
          {
            "label":"No",
            "value":"no"
          },
          {
            "label":"When Low",
            "value":"low"
          }
        ]
      },
      {
        "type": "radiogroup",
        "messageKey": "BluetoothStatus",
        "label": "Display Bluetooth status?",
        "defaultValue": "disconnected",
        "options": [
          {
            "label":"Yes",
            "value":"yes"
          },
          {
            "label":"No",
            "value":"no"
          },
          {
            "label":"When Disconnected",
            "value":"disconnected"
          }
        ]
      },
      {
        "type": "radiogroup",
        "messageKey": "BluetoothDisconnect",
        "label": "Bluetooth Disconnect Vibration?",
        "defaultValue": "yes",
        "options": [
          {
            "label":"Yes",
            "value":"yes"
          },
          {
            "label":"No",
            "value":"no"
          }/*,
          {
            "label":"Message Only",
            "value":"message"
          }*/
        ]
      },
      {
        "type": "radiogroup",
        "messageKey": "BluetoothConnect",
        "label": "Bluetooth Connect Vibration?",
        "defaultValue": "yes",
        "options": [
          {
            "label":"Yes",
            "value":"yes"
          },
          {
            "label":"No",
            "value":"no"
          }/*,
          {
            "label":"Message Only",
            "value":"message"
          }*/
        ]
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];