% gand_get_series  obtain time series from gandalf server
%
% Syntax:
%   res = gand_get_series(handle, symbol, ...);
%
% Input Arguments:
%   handle  - a gandalf connection handle as obtained by gand_open()
%   symbol  - time series symbol to obtain
%    ...    - more symbols
%
% Output Arguments:
%   res     - result struct, containing the slots:
%             .syms  a cell array of symbols
%             .data  a cell array of vectors of 3-tuples:
%                    [date, valflav index, (numerical) value]
%                    the i-th cell of the array corresponds to
%                    the i-th symbol
%             .flds  a cell array of cell arrays of field names
%                    (value flavours or valflavs)
%
% Copyright (C) 2011-2014  Sebastian Freundt <freundt@ga-group.nl>
%
% This file is part of gandalf.
