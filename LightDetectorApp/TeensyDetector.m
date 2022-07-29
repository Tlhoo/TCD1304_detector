classdef TeensyDetector < handle
    %Teensy Detector is a class that connects and reads tcd1304 data via
    %teensy, and plots them on a suitable scale. It is a handle as it
    %interacts with the LightDetectorApp, hence an adress is needed to
    %reference to the same object of TeensyDetector.

    properties
        sys;    %self reference
        serName; % name of serial port. e.g'COM3'
        serPointer; % object of opened serial port
        PIXELS; % PIXEL size 3648
        baudRate=50000;
        dataSetIndex; %counter used in dataIn function
        dataSet; %ceil(PIXELS/resolutionDivideBy)
        dataIn_hold = ''; % used to hold data points that are not full
        dataInTimer; %timer function to loop self.dataIn
        lastRxTime = 0;
        Connected = false; % boolean for whether teensy is connected
        ToCalibrate=false; % boolean for to call calibrate function
        darkAverage = 3500; % default value
        bufferClearTimer; % timer function to clear data in buffer
    end

    methods

        function self = TeensyDetector(sys)
            % Link up buttons and assign functions to buttons.
            self.sys = sys; %in LightDetector, TeensyDetector(app) is called, hence self is app
            self.dataSet = zeros(self.PIXELS+10,1);%PIXELS + 10 because of 10 extra data points of 255 (in arduino)
            self.dataSetIndex=1;
            self.PIXELS=3648; %will have to floor if not an integer            
            axis(self.sys.UIAxes, [0,self.PIXELS,0,3500]); %sets the default UIAxes axis and put on hold
        end
        function Calibrate(self,data) %calibrate value by placing ccd in dark area
            Sum=0;
            for i = 1:numel(data)-200 %??? last few data points in sum are NaN values numel(data). If anyone figures why please let me know
                Sum = Sum + data(i); 
            end
            self.darkAverage = self.darkAverage-floor(Sum/numel(data));
        end
        function Load(self,data) %Loads one set of data into UIAxes. Called by dataIn

            junk_data_end = 10; %remove junk data from last few points
            dataToStartFrom=1; %remove junk data from first few points
            data_cleaned=data(dataToStartFrom:size(data)-junk_data_end);
            data_cleaned=arrayfun(@(x) self.darkAverage-x, data_cleaned); %flips the graph
            if (self.ToCalibrate==true)  % we use calibration here because 1 full set of data can only be obtained here
                disp("place detector in dark area for calibration")
                self.Calibrate(data_cleaned); %!!!!
                self.ToCalibrate=false;
                disp("calibrated");
            end
            cla(self.sys.UIAxes);   %clear axis
            plot(self.sys.UIAxes,1:size(data_cleaned,1),data_cleaned);
        end
        function Connect(self) % find, connect and read port. Also initiate timers
            self.serName=serialportlist("available");
            self.serPointer=serialport(self.serName,self.baudRate);
            self.Connected=true;
            disp("connected");
            self.dataInTimer = timer('Name','DataIn','TimerFcn',@self.dataIn,'StartDelay',0.1,...
                'Period',0.1,'ExecutionMode','fixedSpacing');
            self.bufferClearTimer=timer('Name','bufferTimer','TimerFcn',@self.flushBuffer,...
                'StartDelay',10,'Period',10,'ExecutionMode','fixedSpacing');
            start(self.dataInTimer);
            start(self.bufferClearTimer);
            self.lastRxTime = tic;
        end
        function Disconnect(self) %disconnect from port
            if (self.Connected==true)
                stop(self.dataInTimer);
                stop(self.bufferClearTimer);
                self.serPointer.delete;
                %             closePort(self.serPointer);
                self.Connected=false;
                disp("port disconnected");
            else
                disp('not connected');
            end
        end

        function dataIn(self,~,~) % Main code that runs in the timer function. Converts binary data sent over in chunks to sets of 3648 double datas, and sends them to load.
            %             [~ , leftOverBytesInBuffer] = readPort(self.serPointer, 0);
            t = tic;
            while self.serPointer.NumBytesAvailable > 0 && toc(t) < 0.1
                dataIn=self.serPointer.read(15000,"uint8"); %data comes in as ASCII values
                dataIn(dataIn == 13) = []; %[] represents null value. removes return(<<) symbol
                dataC = strsplit(char(dataIn),'\n'); %converts ASCII to char e.g '7'
                dataC{1} = [self.dataIn_hold dataC{1}]; %combines last cell plus 1st cell in case byte got broken
                self.dataIn_hold = '';
                if dataIn(end) ~= 10 % Wait for completion if last item not '\n'
                    self.dataIn_hold = dataC{end}; 
                     dataC(end) = []; %removes dataC{end}. Logic makes sense since that data will be added to first value in next dataset
                else
                    self.dataIn_hold = '';
                end
                for i = 1:numel(dataC)
                    dataCn=string(dataC{i});
                    if (self.dataSetIndex==self.PIXELS+11) %Load is put here instead of the delimiter, as it prevents the first load to be below self.PIXELS+11 pixels, so it appears nicer on the app.
                        %                             self.dataSet=arrayfun(@str2double,self.dataSet);
                        self.Load(self.dataSet'); %delimeter indicates sending 1 set of data
                    end
                    if contains(dataCn,">>")
                        self.dataSet(:) = 0;
                        self.dataSetIndex=1; %resets quantities after delimiter appears
                    elseif ~isempty(dataCn)

                        self.dataSet(self.dataSetIndex)=dataCn; %store in dataset
                        self.dataSetIndex = self.dataSetIndex + 1;
                    end
%                     if self.dataSetIndex > self.PIXELS+10 %extra data after 255 data is popping up(no idea why, this if statement removes them)
%                         self.dataSet(self.PIXELS+11:end)=[];
% %                         disp(['EXTRA DATA FOUND, CLEANING UP']);
%                     end
                end

            end

        end
        function flushBuffer(self,~,~) % Function that BufferTimer runs. serialPort.NumBytesAVaiable not used as a buffer clear indiccator because it took very long
            flush(self.serPointer); %can add a print line to indicate buffer's being cleared
        end
        function dataInErrorFcn(self,~,~) %
            % Error in executing dataIn.
            self.sys.updateOut('dataInError');
        end
    end
end