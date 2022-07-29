function [COM, errorMsg] = findUSBProdName(prodName)
%findUSBProdName returns serial communication port of specified "Bus
% reported device description"
errorMsg = '';

if exist('jsystem','file')
    fsystem = @jsystem; % Faster DOS command output
else
    fsystem = @system;
end

% Find all the active COM ports
com = 'REG QUERY HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM';
[err,str] = fsystem(com);
if err
    errorMsg = 'Error in executing Registry Query';
    error('Error when executing the system command "%s"',com);
end

% Find the friendly names of all devices
ports = regexp(str,'\\Device\\(?<type>[^ ]*) *REG_SZ *(?<port>COM\d+)','names','dotexceptnewline');
cmd = 'REG QUERY HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ /s /f "FriendlyName" /t "REG_SZ"';
[~,str] = fsystem(cmd);  % 'noshell'

% Get the friendly names of all active COM port devices
names = regexp(str,'FriendlyName *REG_SZ *(?<name>.*?) \((?<port>COM\d+)\)','names','dotexceptnewline');
[i,j] = ismember({ports.port},{names.port});
[ports(i).name] = names(j(i)).name;

% Get the "Bus reported device description" of all active COM port devices
for i = 1:length(ports)
    cmd = sprintf('powershell -command "(Get-WMIObject Win32_PnPEntity | where {$_.name -match ''(%s)''}).GetDeviceProperties(''DEVPKEY_Device_BusReportedDeviceDesc'').DeviceProperties.Data"',ports(i).port);
    [~,ports(i).USBDescriptorName] = fsystem(cmd);
end

% Find USBDrescriptorName that matches
[~,j2] = ismember(prodName, {ports.USBDescriptorName});
if j2
    COM = ports(j2).port;
else
    errorMsg = 'Error: USB Descriptor Name not found.';
    error('COM Port with USBDescriptorName "%s" not found.',prodName);
end
end