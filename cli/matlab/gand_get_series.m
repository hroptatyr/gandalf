% gand_get_series  obtain time series from gandalf server
%
% Syntax:
%   d = gand_get_series(handle, symbol, valflav, ...);
%   [d, p] = gand_get_series(handle, symbol, valflav, ...);
%   [d, p, fields] = gand_get_series(handle, symbol, valflav, ...)
%   [d, p, fields, raw] = gand_get_series(handle, symbol, valflav, ...)
%
% Input Arguments:
%   handle  - a gandalf connection handle as obtained by gand_open()
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
% Copyright (C) 2011-2013  Sebastian Freundt <freundt@ga-group.nl>
%
% This file is part of gandalf.
