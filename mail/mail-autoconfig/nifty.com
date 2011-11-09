<?xml version="1.0" encoding="UTF-8"?>
<clientConfig>
  <emailProvider id="nifty.com">
    <domain>nifty.com</domain>
    <displayName>@nifty</displayName>
    <displayShortName>@nifty</displayShortName>
    <incomingServer type="pop3">
      <hostname>pop.nifty.com</hostname>
      <port>110</port>
      <socketType>plain</socketType>
      <username>%EMAILLOCALPART%</username>
      <authentication>plain</authentication>
    </incomingServer>
    <outgoingServer type="smtp">
      <hostname>smtp.nifty.com</hostname>
      <port>587</port>
      <socketType>plain</socketType>
      <username>%EMAILLOCALPART%</username>
      <authentication>plain</authentication>
      <addThisServer>true</addThisServer>
      <useGlobalPreferredServer>false</useGlobalPreferredServer>
    </outgoingServer>
  </emailProvider>
</clientConfig>
