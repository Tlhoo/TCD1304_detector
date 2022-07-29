function assignCallbacks(app)
    app.ConnectButton.ButtonPushedFcn = @connectButtonF; % note XButton, X is the name of the button defined in app designer
    app.DisconnectButton.ButtonPushedFcn = @disconnectButtonF;
    app.CalibrateButton.ButtonPushedFcn = @CalibrateButtonF;
    function connectButtonF(src,b)        
        app.TD.Connect();
    end
    
    function disconnectButtonF(src,b)
        app.TD.Disconnect();
    end
    function CalibrateButtonF(src,b)
        app.TD.ToCalibrate=true;
    end
end