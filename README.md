# WeebSocket project

A project that allows you to interact with another person's computer through a web interface for you. Yes, you - the one who's suffering with Computer Networking.

## Table of contents

- [Features](#features)
- 

## Features

- Managing processes: list, start, stop processes;
- Managing applications: list, start, stop applications;
- Take a screenshot;
- Keylogger;
-

## Diagram:
flowchart LR
    %% Clients
    subgraph Clients["Client Devices (Same Network)"]
        C1["Browser / App"]
    end

    %% Interfaces
    subgraph Interface["Communication Layer"]
        HTTP["HTTP Server\nPort 8000\n(UI Delivery)"]
        WS["WebSocket Server\nPort 8080\n(Command Channel)"]
    end

    %% Core
    subgraph Core["Application Core"]
        Dispatcher["Command Dispatcher\nhandleClient()"]

        PM["ProcessManager\n- list/start/stop"]
        CS["CaptureScreen\n- screenshot"]
        SR["ScreenRecorder\n- async recording"]
        WL["Window & App Listing\n- running apps\n- start menu"]
    end

    %% OS
    subgraph OS["Windows OS / Native APIs"]
        WinAPI["Win32 / PSAPI"]
        GDI["GDI / Desktop APIs"]
        IPHLP["IP Helper API"]
    end

    %% Connections
    C1 -->|HTTP 8000| HTTP
    C1 -->|WebSocket 8080| WS

    WS --> Dispatcher

    Dispatcher --> PM
    Dispatcher --> CS
    Dispatcher --> SR
    Dispatcher --> WL

    PM --> WinAPI
    CS --> GDI
    SR --> GDI
    WL --> WinAPI
    HTTP --> IPHLP

