// New in version 2.0.0
// Implemented RTOS queue and deleted sync events
// Added a STATIC class Synchronous Queue.
// Added mode selection to change how events are actioned (but no change in detections)
// All external functions are executed outside of ISR and in RTOS scope, so no need for IRAM

#ifndef InterruptButton_h
#define InterruptButton_h

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define ASYNC_EVENT_QUEUE_DEPTH   5     // This queue is service very quickly so can be short
#define SYNC_EVENT_QUEUE_DEPTH    10    // This queue is limited to mainloop frequency so actions can backup

typedef void (*func_ptr_t)(void);       // Type def to faciliate manage pointers to external action functions

enum modes {
  Mode_Asynchronous,                    // All actions performed via Asynchronous RTOS queue
  Mode_Hybrid,                          // keyUp and keyDown performed by RTOS queue, remaining actions by Static Synchronous Queue.
  Mode_Synchronous                      // All actions performed by Static Synchronous Queue.
};

enum events:uint8_t {
  Event_KeyDown = 0,
  Event_KeyUp,
  Event_KeyPress,
  Event_LongKeyPress,
  Event_AutoRepeatPress,
  Event_DoubleClick,
  NumEventTypes,                        // Not an event, but this value used to size the number of columns in event/action array.
  Event_All
};


// -- Interrupt Button and Debouncer ---------------------------------------------------------------------------------------
// -- ----------------------------------------------------------------------------------------------------------------------
class InterruptButton {
  private:
    enum buttonStates {                 // Enumeration to assist with program flow at state machine for reading button
      Released, 
      ConfirmingPress,
      Pressing,
      Pressed, 
      WaitingForRelease,
      Releasing
    };

    // STATIC class members shared by all instances of this object (common across all instances of the class)
    // ------------------------------------------------------------------------------------------------------
    static void queueServicer(void* pvParams);                              // Function used as RTOS task to receive and process action from RTOS message queue.
    static void readButton(void* arg);                                      // function to read button state (must be static to bind to GPIO and timer ISR)
    static void longPressEvent(void *arg);                                  // Callback to excecute a longPress event, called by timer
    static void autoRepeatPressEvent(void *arg);                            // Callback to excecute a autoRepeatPress event, called by timer
    static void doubleClickTimeout(void *arg);                              // Callback used to separate double-clicks from regular keyPress's, called by timer
    static void startTimer(esp_timer_handle_t &timer,                       // Helper func to start timer.
                           uint32_t duration_US, 
                           void (*callBack)(void* arg),
                           InterruptButton* btn, 
                           const char *msg); 
    static void killTimer(esp_timer_handle_t &timer);                       // Helper function to kill a timer

    static const uint8_t  m_targetPolls = 10;                               // Desired number of polls required to debounce a button
    static bool           m_classInitialised;                               // Boolean flag to control class initialisation
    static QueueHandle_t  asyncEventQueue;                                  // Pointer/handle to the RTOS message queue handling actions.
    static TaskHandle_t   queueServicerHandle;                              // Pointer/handle to the RTOS task that actions the RTOS Queue messages
    static func_ptr_t     syncEventQueue[];                                 // Array used as the Static Synchronous Event Queue
    static bool           m_firstButtonInitialised;                         // Used to block any further changes to m_numMenus
    static uint8_t        m_numMenus;                                       // Total number of menu sets, can be set by user, but only before initialising first button
    static uint8_t        m_menuLevel;                                      // Current menulevel for all buttons (global in class so common across all buttons)
    static modes          m_mode;

    // Non-static instance specific member declarations
    // ------------------------------------------------
    void                  initialiseInstance(void);                         // Setup interrupts and event-action array

    bool                  m_thisButtonInitialised = false;                  // Allows us to intialise when binding functions (ie detect if already done)
    gpio_num_t            m_pin;                                            // Button gpio
    uint8_t               m_pressedState;                                   // State of button when it is pressed (LOW or HIGH)
    gpio_mode_t           m_pinMode;                                        // GPIO mode: IDF's input/output mode
    volatile buttonStates m_state;                                          // Instance specific state machine variable (intialised when intialising button)
    volatile bool         m_wtgForDblClick = false;
    esp_timer_handle_t    m_buttonPollTimer;                                // Instance specific timer for button debouncing
    esp_timer_handle_t    m_buttonLPandRepeatTimer;                         // Instance specific timer for button longPress and autoRepeat timing
    esp_timer_handle_t    m_buttonDoubleClickTimer;                         // Instance specific timer for discerning double-clicks from regular keyPresses

    volatile uint8_t      m_doubleClickMenuLevel;                           // Stores current menulevel while differentiating between regular keyPress or a double-click
    uint16_t              m_pollIntervalUS;                                 // Timing variables
    uint16_t              m_longKeyPressMS;
    uint16_t              m_autoRepeatMS;           
    uint16_t              m_doubleClickMS;
     
    volatile bool         m_longPress_preventKeyPress;                      // Boolean flag to prevent firing a keypress if a long press occurred (outside of polling fuction)
    volatile uint16_t     m_validPolls = 0;                                 // Variables to conduct debouncing algoritm
    volatile uint16_t     m_totalPolls = 0;

    func_ptr_t**          eventActions = nullptr;                           // Pointer to 2D array, event actions by row, menu levels by column.
    uint16_t              eventMask = 0b0000010000111;                      // Default to keyUp, keyDown, and keyPress enabled, and no blanked disable
                                                                            // When binding functions, longKeyPress, autoKeyPresses, & double-clicks are automatically enabled.

  public:
    // Static class members shared by all instances of this object -----------------------
    static bool     setMode(modes mode);                              // Toggle between Synchronous (Static Queue), Hybrid, or Asynchronous modes (RTOS Queue)
    static modes    getMode(void);
    static void     processSyncEvents(void);                          // Process Sync Events, called from main looop
    static void     setMenuCount(uint8_t numberOfMenus);              // Sets number of menus/pages that each button has (can only be done before intialising first button)
    static uint8_t  getMenuCount(void);                               // Retrieves total number of menus.
    static void     setMenuLevel(uint8_t level);                      // Sets menu level across all buttons (ie buttons mean something different each page)
    static uint8_t  getMenuLevel();                                   // Retrieves menu level


    // Non-static instance specific member declarations ----------------------------------
    InterruptButton(uint8_t pin,                                      // Class Constructor, pin to monitor
                    uint8_t pressedState,                             // State of the pin when pressed (HIGH or LOW)
                    gpio_mode_t pinMode = GPIO_MODE_INPUT,            
                    uint16_t longKeyPressMS = 750, 
                    uint16_t autoRepeatMS =   250,
                    uint16_t doubleClickMS =  333, 
                    uint32_t debounceUS =     8000);
    ~InterruptButton();                                               // Class Destructor

    void            enableEvent(events event);                        // Enable the event passed as argument (updates bitmask)
    void            disableEvent(events event);                       // Disable the event passed as argument (updates bitmask)
    bool            eventEnabled(events event);                       // Read bitmask and determine if event is enabled
    void            setLongPressInterval(uint16_t intervalMS);        // Updates LongPress Interval
    uint16_t        getLongPressInterval(void);
    void            setAutoRepeatInterval(uint16_t intervalMS);       // Updates autoRepeat Interval
    uint16_t        getAutoRepeatInterval(void);
    void            setDoubleClickInterval(uint16_t intervalMS);      // Updates autoRepeat Interval
    uint16_t        getDoubleClickInterval(void);


    // Routines to manage interface with external action functions associated with each event ---
    void            bind(events     event,                                  // Used to bind an action to an event at a given menulevel
                         uint8_t    menuLevel, 
                         func_ptr_t action);                                 
    inline void     bind(events event, func_ptr_t action) { bind(event, m_menuLevel, action); } // Above function defaulting to current menulevel

    void            unbind(events   event,                                  // Used to unbind an action to an event at a given menulevel
                           uint8_t  menuLevel);
    inline void     unbind(events event) { unbind(event, m_menuLevel); };   // Above function defaulting to current menulevel

    void            action(events     event,                                // Helper function to simplify calling actions at specified menulevel
                           uint8_t    menuLevel, 
                           BaseType_t *pxHigherPriorityTaskWoken = nullptr); 
    inline void     action(events event, BaseType_t *pxHigherPriorityTaskWoken = nullptr) { action(event, m_menuLevel, pxHigherPriorityTaskWoken); };
};

#endif
