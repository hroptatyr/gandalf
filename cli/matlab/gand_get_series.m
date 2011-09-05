% gand_get_series  obtain time series from gandalf server
%
% Syntax:
%   d = gand_get_series(service, symbol, valflav, ...);
%   [d, p] = gand_get_series(service, symbol, valflav, ...);
%   [d, p, fields] = gand_get_series(service, symbol, valflav, ...)
%
% Input Arguments:
%   service - a gandalf service string in the form HOST:PORT or PATH
%             where the former describes the tcp service running on
%             HOST on port PORT and the latter describes the path
%             to the unix domain socket of the gandalf service
%   symbol  - time series symbol to obtain
%
% Optional Input Arguments:
%   valflav - a valflav string to be filtered, may be given in gandalf
%             alternative syntax, e.g. 'fix/stl/close'
%
% Output Arguments:
%   d      - vector of dates in the result
%   p      - matrix of prices, one column per valflav
%   fields - cell array of matching valflav strings
%
% Copyright (C) 2011  Sebastian Freundt <freundt@ga-group.nl>
%
% This file is part of gandalf.
